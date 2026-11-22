#include "System/ModelRender.h"

#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace ModelRender
{
	namespace
	{
		// ---- the meshes, read off the model -------------------------------

		struct Vertex
		{
			float px, py, pz;
			float u, v;
			float nx, ny, nz;
			float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;  // the vertex colour, which hair takes its tint by
		};

		struct Mesh
		{
			ComPtr<ID3D11Buffer>             vertexBuffer;
			ComPtr<ID3D11Buffer>             indexBuffer;
			ComPtr<ID3D11ShaderResourceView> diffuse;
			ComPtr<ID3D11ShaderResourceView> faceTint;                  // FaceGen's tint texture, laid over the face's own
			ComPtr<ID3D11ShaderResourceView> detail;                    // FaceGen's detail texture, which the tinted face is scaled by
			ComPtr<ID3D11ShaderResourceView> normalMap;                 // a model space one, as a head's is, turned into the model's by normalTurn,
			bool                             tangentNormals   = false;  // or a tangent space one, shaped by the UVs, as an item's is
			std::array<float, 4>             uvTransform{ 0.0f, 0.0f, 1.0f, 1.0f };  // the material's offset then scale
			bool                             envUntinted      = false;  // reflects as glass, e.g. an eye's cornea
			ComPtr<ID3D11ShaderResourceView> glow;                      // where it glows, lit by its emissive colour
			ComPtr<ID3D11ShaderResourceView> envMap;                    // a cube map it reflects, as metal does
			ComPtr<ID3D11ShaderResourceView> envMask;                   // where it reflects, else everywhere
			bool                             envMaskInAlpha   = false;  // in its alpha, e.g. a normal map's gloss, rather than its red
			float                            envScale         = 0.0f;
			std::array<float, 9>             normalTurn{ 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
			bool                             faceGen          = false;
			bool                             plain            = false;  // a caller's own, without vertex colours
			float                            specularStrength = 0.10f;
			float                            specularPower    = 42.0f;
			std::array<float, 3>             emissive{};
			float                            emissiveStrength = 0.0f;
			std::uint32_t                    indexCount       = 0;
			std::array<float, 3>             tint{ 1.0f, 1.0f, 1.0f };  // hair's tint colour
			bool                             hasTint          = false;
			bool                             useAlpha         = false;
			bool                             blendAlpha       = false;
			bool                             split            = false;  // blended but cut too, so drawn solid where strong and blended where faint
			float                            alpha            = 1.0f;
			float                            cutoff           = 0.0f;
			std::array<float, 3>             min{};
			std::array<float, 3>             max{};
			bool                             hasBounds        = false;
			bool                             twoSided         = true;   // drawn from behind too, else only its side facing the camera
			bool                             vertexAlpha      = false;  // its vertex colours' alpha fades it too
			bool                             effect           = false;  // drawn unlit, as an effect shader's glowing shell
			std::array<float, 4>             effectColour{ 1.0f, 1.0f, 1.0f, 1.0f };
			float                            effectScale      = 1.0f;
			ComPtr<ID3D11ShaderResourceView> palette;                   // what the diffuse's green picks its colour from
			bool                             paletteAlpha     = false;
			bool                             falloff          = false;
			std::array<float, 4>             falloffData{ 1.0f, 0.0f, 1.0f, 1.0f };  // the two cosines, then the opacity at each
			bool                             additive         = false;
		};

		struct Part
		{
			RE::BSGeometry* geometry = nullptr;
			RE::NiTransform transform;  // the shape's place in the model's own space, for an unskinned shape
			RE::NiTransform toModel;    // the world to the model's space, which a skinned shape's bones are taken through
		};

		// Every shape that's shown, placed as the game has it, in the model's own space so its front is the model's
		void Collect(RE::NiAVObject* a_object, const RE::NiTransform& a_toModel, std::vector<Part>& a_out)
		{
			if (!a_object || a_object->GetAppCulled())
				return;
			if (auto* geometry = a_object->AsGeometry()) {
				a_out.push_back({ geometry, a_toModel * geometry->world, a_toModel });
				return;
			}
			if (auto* node = a_object->AsNode()) {
				for (auto& child : node->GetChildren())
					Collect(child.get(), a_toModel, a_out);
			}
		}

		void Include(Mesh& a_mesh, const RE::NiPoint3& a_p)
		{
			if (!a_mesh.hasBounds) {
				a_mesh.min = a_mesh.max = { a_p.x, a_p.y, a_p.z };
				a_mesh.hasBounds        = true;
				return;
			}
			a_mesh.min = { std::min(a_mesh.min[0], a_p.x), std::min(a_mesh.min[1], a_p.y), std::min(a_mesh.min[2], a_p.z) };
			a_mesh.max = { std::max(a_mesh.max[0], a_p.x), std::max(a_mesh.max[1], a_p.y), std::max(a_mesh.max[2], a_p.z) };
		}

		RE::NiPoint3 TurnNormal(const RE::NiTransform& a_xf, const std::uint8_t* a_n)
		{
			const auto&  r = a_xf.rotate.entry;
			const float  x = a_n[0] / 127.5f - 1.0f, y = a_n[1] / 127.5f - 1.0f, z = a_n[2] / 127.5f - 1.0f;
			RE::NiPoint3 n{ r[0][0] * x + r[0][1] * y + r[0][2] * z, r[1][0] * x + r[1][1] * y + r[1][2] * z, r[2][0] * x + r[2][1] * y + r[2][2] * z };
			const float  len = n.Length();
			return len > 1e-6f ? n / len : RE::NiPoint3{ 0.0f, 0.0f, 1.0f };
		}

		// Its colour only where its shader asks for it, as the game shades by them only then, else white. Its alpha always, for a shape
		// that fades by its vertices
		void ReadColour(const std::uint8_t* a_vertex, std::uint32_t a_offset, bool a_takesColour, Vertex& a_out)
		{
			if (a_takesColour)
				a_out.r = a_vertex[a_offset] / 255.0f, a_out.g = a_vertex[a_offset + 1] / 255.0f, a_out.b = a_vertex[a_offset + 2] / 255.0f;
			a_out.a = a_vertex[a_offset + 3] / 255.0f;
		}

		// Whether a shape's shader shades it by its vertex colours. Hair always takes them, as its tint is laid on where their green says
		bool TakesVertexColours(RE::BSGeometry* a_geometry)
		{
			auto* shader = a_geometry->GetGeometryRuntimeData().shaderProperty.get();
			if (!shader)
				return false;
			auto* lighting = netimmerse_cast<RE::BSLightingShaderProperty*>(shader);
			if (lighting && lighting->material && lighting->material->GetFeature() == RE::BSShaderMaterial::Feature::kHairTint)
				return true;
			return shader->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kVertexColors);
		}

		void SetTurn(const RE::NiTransform& a_xf, Mesh& a_out)
		{
			for (int r = 0; r < 3; ++r) {
				for (int c = 0; c < 3; ++c)
					a_out.normalTurn[r * 3 + c] = a_xf.rotate.entry[r][c];
			}
		}

		RE::NiPoint3 ReadPosition(const std::uint8_t* a_vertex, std::uint32_t a_offset, bool a_full)
		{
			if (a_full) {
				const auto* p = reinterpret_cast<const float*>(a_vertex + a_offset);
				return { p[0], p[1], p[2] };
			}
			const auto* h = reinterpret_cast<const std::uint16_t*>(a_vertex + a_offset);
			return { DirectX::PackedVector::XMConvertHalfToFloat(h[0]), DirectX::PackedVector::XMConvertHalfToFloat(h[1]), DirectX::PackedVector::XMConvertHalfToFloat(h[2]) };
		}

		// Whether a position is three floats rather than four halves: what follows it starts 12 bytes on or more
		bool FullPrecision(RE::BSGraphics::VertexDesc a_desc, std::uint32_t a_offset)
		{
			using V                     = RE::BSGraphics::Vertex;
			std::uint32_t next          = a_desc.GetSize();
			const V::Attribute others[] = { V::VA_TEXCOORD0, V::VA_TEXCOORD1, V::VA_NORMAL, V::VA_BINORMAL, V::VA_COLOR, V::VA_SKINNING, V::VA_EYEDATA };
			for (const auto attr : others) {
				const auto off = a_desc.GetAttributeOffset(attr);
				if (off > a_offset && off < next)
					next = off;
			}
			return next - a_offset >= 12;
		}

		// FaceGen's morphed positions, which stand in for the buffer's own on a head
		std::span<const DirectX::XMFLOAT4> DynamicPositions(RE::BSGeometry* a_geometry, std::uint32_t a_count)
		{
			auto* shape   = a_geometry->AsTriShape();
			auto* dynamic = shape ? shape->AsDynamicTriShape() : nullptr;
			if (!dynamic || a_count == 0)
				return {};
			auto& data = dynamic->GetDynamicTrishapeRuntimeData();
			if (!data.dynamicData || data.dataSize < a_count * sizeof(DirectX::XMFLOAT4))
				return {};
			return { static_cast<const DirectX::XMFLOAT4*>(data.dynamicData), a_count };
		}

		bool Upload(ID3D11Device* a_device, const std::vector<Vertex>& a_vertices, const std::vector<std::uint32_t>& a_indices, Mesh& a_out)
		{
			if (a_vertices.empty() || a_indices.empty())
				return false;

			D3D11_BUFFER_DESC      desc{ static_cast<UINT>(a_vertices.size() * sizeof(Vertex)), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER };
			D3D11_SUBRESOURCE_DATA data{ a_vertices.data() };
			if (FAILED(a_device->CreateBuffer(&desc, &data, a_out.vertexBuffer.GetAddressOf())))
				return false;

			desc = { static_cast<UINT>(a_indices.size() * sizeof(std::uint32_t)), D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER };
			data = { a_indices.data() };
			if (FAILED(a_device->CreateBuffer(&desc, &data, a_out.indexBuffer.GetAddressOf())))
				return false;
			a_out.indexCount = static_cast<std::uint32_t>(a_indices.size());
			return true;
		}

		// A skinned shape keeps a CPU copy of its vertices in its skin partition, which a head's are read from. Each vertex is posed
		// as the game poses it, by its bones' places blended by its weights
		bool FromSkinPartition(ID3D11Device* a_device, const Part& a_part, RE::NiSkinInstance& a_instance, RE::NiSkinPartition& a_skin, Mesh& a_out)
		{
			const auto dynamic     = DynamicPositions(a_part.geometry, a_skin.vertexCount);

			const bool takesColour = TakesVertexColours(a_part.geometry);

			// Skin space to the model's, one per bone
			std::vector<RE::NiTransform> bones;
			if (auto* data = a_instance.skinData.get(); data && data->GetBoneCount() > 0 && a_instance.bones) {
				bones.resize(data->GetBoneCount());
				for (std::uint32_t i = 0; i < data->GetBoneCount(); ++i) {
					auto* bone = a_instance.bones[i];
					bones[i]   = bone ? a_part.toModel * (bone->world * data->GetBoneDataSkinToBone(i)) : a_part.transform;
				}
			}

			std::vector<Vertex>        vertices;
			std::vector<std::uint32_t> indices;
			std::vector<std::uint32_t> uses(bones.size());  // vertices each bone leads, so a model space normal map turns with the head
			for (std::uint32_t pi = 0; pi < a_skin.numPartitions; ++pi) {
				auto& p    = a_skin.partitions[pi];
				auto* buff = p.buffData;
				if (!buff || !buff->rawVertexData || !p.triList || p.triangles == 0)
					continue;

				using F                    = RE::BSGraphics::Vertex::Flags;
				const std::uint32_t stride = buff->vertexDesc.GetSize();
				const bool          hasPos = buff->vertexDesc.HasFlag(F::VF_VERTEX);
				const bool          hasUV  = buff->vertexDesc.HasFlag(F::VF_UV);
				const bool          hasNrm = buff->vertexDesc.HasFlag(F::VF_NORMAL);
				const bool          hasCol = buff->vertexDesc.HasFlag(F::VF_COLORS);
				if (stride == 0 || (!hasPos && dynamic.empty()))
					continue;

				const auto posOffset  = buff->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_POSITION);
				const auto uvOffset   = buff->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0);
				const auto nrmOffset  = buff->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_NORMAL);
				const auto skinOffset = buff->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING);
				const auto colOffset  = buff->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_COLOR);
				const bool full       = hasPos && FullPrecision(buff->vertexDesc, posOffset);
				const bool skinned    = buff->vertexDesc.HasFlag(F::VF_SKINNED) && p.bones && !bones.empty();

				// As many vertices as the triangles use, never more than the buffer or the morphs hold
				std::uint32_t count = 0;
				for (std::uint32_t t = 0; t < p.triangles * 3u; ++t)
					count = std::max<std::uint32_t>(count, p.triList[t] + 1u);
				count = std::min(count, a_skin.vertexCount);
				if (!hasPos)
					count = std::min(count, static_cast<std::uint32_t>(dynamic.size()));
				if (count == 0)
					continue;

				const auto base = static_cast<std::uint32_t>(vertices.size());
				for (std::uint32_t i = 0; i < count; ++i) {
					const auto*  v   = buff->rawVertexData + static_cast<std::size_t>(i) * stride;
					RE::NiPoint3 raw = i < dynamic.size() ? RE::NiPoint3{ dynamic[i].x, dynamic[i].y, dynamic[i].z } : ReadPosition(v, posOffset, full);

					// Four half-float weights then four bone indices into the partition's own bone list
					const RE::NiTransform* main = &a_part.transform;
					RE::NiPoint3           pos  = a_part.transform * raw;
					if (skinned) {
						const auto*  weights = reinterpret_cast<const std::uint16_t*>(v + skinOffset);
						const auto*  boneIdx = v + skinOffset + 8;
						RE::NiPoint3 sum{};
						float        total = 0.0f, heaviest = 0.0f;
						std::size_t  lead  = bones.size();
						for (int k = 0; k < 4; ++k) {
							const float w = DirectX::PackedVector::XMConvertHalfToFloat(weights[k]);
							if (w <= 0.000001f || boneIdx[k] >= p.numBones)
								continue;
							const auto bone = p.bones[boneIdx[k]];
							if (bone >= bones.size())
								continue;
							sum += (bones[bone] * raw) * w;
							total += w;
							if (w > heaviest) {
								heaviest = w;
								main     = &bones[bone];
								lead     = bone;
							}
						}
						if (total > 0.0f)
							pos = sum / total;
						if (lead < uses.size())
							++uses[lead];
					}
					Include(a_out, pos);

					Vertex out{ pos.x, pos.y, pos.z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
					if (hasUV) {
						const auto* uv = reinterpret_cast<const std::uint16_t*>(v + uvOffset);
						out.u          = DirectX::PackedVector::XMConvertHalfToFloat(uv[0]);
						out.v          = DirectX::PackedVector::XMConvertHalfToFloat(uv[1]);
					}
					if (hasNrm) {
						const auto n = TurnNormal(*main, v + nrmOffset);
						out.nx = n.x, out.ny = n.y, out.nz = n.z;
					}
					if (hasCol)
						ReadColour(v, colOffset, takesColour, out);
					vertices.push_back(out);
				}
				for (std::uint32_t t = 0; t < p.triangles * 3u; ++t)
					indices.push_back(base + (p.triList[t] < count ? p.triList[t] : 0u));
			}
			const auto lead = std::ranges::max_element(uses);
			SetTurn(lead != uses.end() && *lead > 0 ? bones[lead - uses.begin()] : a_part.transform, a_out);
			return Upload(a_device, vertices, indices, a_out);
		}

		// An unskinned shape's vertices only live on the GPU, so they're copied back, which waits on it
		bool FromRendererData(ID3D11Device* a_device, ID3D11DeviceContext* a_context, const Part& a_part, Mesh& a_out)
		{
			auto* shape = a_part.geometry->AsTriShape();
			auto& rt    = a_part.geometry->GetGeometryRuntimeData();
			auto* data  = rt.rendererData;
			if (!shape || !data || !data->vertexBuffer || !data->indexBuffer)
				return false;

			auto*               vb     = reinterpret_cast<ID3D11Buffer*>(data->vertexBuffer);
			auto*               ib     = reinterpret_cast<ID3D11Buffer*>(data->indexBuffer);
			const std::uint32_t stride = rt.vertexDesc.GetSize();
			if (stride == 0)
				return false;

			const auto readBack = [&](ID3D11Buffer* a_buffer, std::vector<std::uint8_t>& a_out) {
				D3D11_BUFFER_DESC desc{};
				a_buffer->GetDesc(&desc);
				desc.Usage          = D3D11_USAGE_STAGING;
				desc.BindFlags      = 0;
				desc.MiscFlags      = 0;
				desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
				ComPtr<ID3D11Buffer> staging;
				if (FAILED(a_device->CreateBuffer(&desc, nullptr, staging.GetAddressOf())))
					return false;
				a_context->CopyResource(staging.Get(), a_buffer);
				D3D11_MAPPED_SUBRESOURCE mapped{};
				if (FAILED(a_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
					return false;
				a_out.assign(static_cast<const std::uint8_t*>(mapped.pData), static_cast<const std::uint8_t*>(mapped.pData) + desc.ByteWidth);
				a_context->Unmap(staging.Get(), 0);
				return true;
			};

			std::vector<std::uint8_t> vbytes, ibytes;
			if (!readBack(vb, vbytes) || !readBack(ib, ibytes))
				return false;

			auto& tri          = shape->GetTrishapeRuntimeData();
			auto  count        = std::min<std::uint32_t>(tri.vertexCount ? tri.vertexCount : 0xFFFFFFFF, static_cast<std::uint32_t>(vbytes.size() / stride));
			const auto dynamic = DynamicPositions(a_part.geometry, count);

			using F              = RE::BSGraphics::Vertex::Flags;
			const bool hasPos    = rt.vertexDesc.HasFlag(F::VF_VERTEX);
			const bool hasUV     = rt.vertexDesc.HasFlag(F::VF_UV);
			const bool hasNrm    = rt.vertexDesc.HasFlag(F::VF_NORMAL);
			const auto posOffset = rt.vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_POSITION);
			const auto uvOffset  = rt.vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0);
			const auto nrmOffset = rt.vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_NORMAL);
			const bool hasCol    = rt.vertexDesc.HasFlag(F::VF_COLORS);
			const auto colOffset = rt.vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_COLOR);
			const bool full      = hasPos && FullPrecision(rt.vertexDesc, posOffset);
			const bool takes     = TakesVertexColours(a_part.geometry);
			if (!hasPos)
				count = std::min(count, static_cast<std::uint32_t>(dynamic.size()));

			std::vector<Vertex> vertices(count);
			for (std::uint32_t i = 0; i < count; ++i) {
				const auto*  v   = vbytes.data() + static_cast<std::size_t>(i) * stride;
				RE::NiPoint3 raw = i < dynamic.size() ? RE::NiPoint3{ dynamic[i].x, dynamic[i].y, dynamic[i].z } : ReadPosition(v, posOffset, full);
				const auto   pos = a_part.transform * raw;
				Include(a_out, pos);
				vertices[i] = { pos.x, pos.y, pos.z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
				if (hasUV) {
					const auto* uv = reinterpret_cast<const std::uint16_t*>(v + uvOffset);
					vertices[i].u  = DirectX::PackedVector::XMConvertHalfToFloat(uv[0]);
					vertices[i].v  = DirectX::PackedVector::XMConvertHalfToFloat(uv[1]);
				}
				if (hasNrm) {
					const auto n   = TurnNormal(a_part.transform, v + nrmOffset);
					vertices[i].nx = n.x, vertices[i].ny = n.y, vertices[i].nz = n.z;
				}
				if (hasCol)
					ReadColour(v, colOffset, takes, vertices[i]);
			}
			SetTurn(a_part.transform, a_out);

			const auto*         src        = reinterpret_cast<const std::uint16_t*>(ibytes.data());
			const std::uint32_t available  = static_cast<std::uint32_t>(ibytes.size() / sizeof(std::uint16_t));
			const std::uint32_t indexCount = std::min(tri.triangleCount ? tri.triangleCount * 3u : available, available);
			std::vector<std::uint32_t> indices(indexCount);
			for (std::uint32_t i = 0; i < indexCount; ++i)
				indices[i] = src[i] < count ? src[i] : 0u;
			return Upload(a_device, vertices, indices, a_out);
		}

		ID3D11ShaderResourceView* TextureSrv(RE::NiTexture* a_texture)
		{
			auto* source = netimmerse_cast<RE::NiSourceTexture*>(a_texture);
			auto* data   = source ? reinterpret_cast<RE::NiTexture::RendererData*>(source->rendererTexture) : nullptr;
			return data ? reinterpret_cast<ID3D11ShaderResourceView*>(data->resourceView) : nullptr;
		}

		// An effect shader's look, from what ReadMeshMaterial read or a caller filled in
		void CopyEffect(const FUCK::MeshDraw& a_in, Mesh& a_out)
		{
			a_out.effect = a_in.effect;
			if (!a_in.effect)
				return;
			a_out.effectColour = { a_in.effectColour[0], a_in.effectColour[1], a_in.effectColour[2], a_in.effectColour[3] };
			a_out.effectScale  = a_in.effectScale;
			a_out.palette      = static_cast<ID3D11ShaderResourceView*>(a_in.palette);
			a_out.paletteAlpha = a_in.paletteAlpha;
			a_out.falloff      = a_in.falloff;
			a_out.falloffData  = { a_in.falloffAngles[0], a_in.falloffAngles[1], a_in.falloffOpacity[0], a_in.falloffOpacity[1] };
			a_out.additive     = a_in.additive;
		}

		// The head's two tints, a model space normal map, and how it's see-through, read here. The rest, the textures and look its
		// material gives, is ReadMeshMaterial's, which Fitting Room's item cards read too, so the two are drawn the same way. Only
		// what this has always drawn is taken from it, as a portrait's lighting is tuned to its own specular
		void ReadMaterial(RE::BSGeometry* a_geometry, Mesh& a_out)
		{
			auto& rt = a_geometry->GetGeometryRuntimeData();

			FUCK::MeshDraw shared;
			if (ReadMeshMaterial(rt.shaderProperty.get(), rt.alphaProperty.get(), shared)) {
				a_out.diffuse        = static_cast<ID3D11ShaderResourceView*>(shared.diffuse);
				a_out.alpha          = shared.alpha;
				a_out.twoSided       = shared.twoSided;
				a_out.hasTint        = shared.useTint;
				a_out.tint           = { shared.tint[0], shared.tint[1], shared.tint[2] };
				a_out.envMap         = static_cast<ID3D11ShaderResourceView*>(shared.envMap);
				a_out.envMask        = static_cast<ID3D11ShaderResourceView*>(shared.envMask);
				a_out.envMaskInAlpha = shared.envMaskInAlpha;
				a_out.envScale       = shared.envScale;
				a_out.envUntinted    = shared.envUntinted;
				// Faded by its vertices where its shader says, as a shaved beard's edge is: without it the stubble stopped in a hard
				// line round the whole mesh, a patch over the jaw
				a_out.vertexAlpha    = shared.vertexAlpha;
				a_out.uvTransform    = { shared.uvOffset[0], shared.uvOffset[1], shared.uvScale[0], shared.uvScale[1] };
				// A tangent space map here; a model space one is read below, turned as the head's bones turn it
				if (shared.normalMap && !shared.normalMapModelSpace) {
					a_out.normalMap      = static_cast<ID3D11ShaderResourceView*>(shared.normalMap);
					a_out.tangentNormals = true;
				}
				a_out.glow           = static_cast<ID3D11ShaderResourceView*>(shared.glow);
				if (a_out.glow) {
					a_out.emissive         = { shared.emissive[0], shared.emissive[1], shared.emissive[2] };
					a_out.emissiveStrength = shared.emissiveStrength;
				}
				CopyEffect(shared, a_out);
			}

			auto* lighting = netimmerse_cast<RE::BSLightingShaderProperty*>(rt.shaderProperty.get());
			if (lighting && lighting->material) {
				auto* material = static_cast<RE::BSLightingShaderMaterialBase*>(lighting->material);
				if (lighting->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals)) {
					a_out.normalMap      = TextureSrv(material->normalTexture.get());
					a_out.tangentNormals = false;
				}
				if (material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGen) {
					a_out.faceTint = TextureSrv(static_cast<RE::BSLightingShaderMaterialFacegen*>(material)->tintTexture.get());
					a_out.detail   = TextureSrv(static_cast<RE::BSLightingShaderMaterialFacegen*>(material)->detailTexture.get());
					a_out.faceGen  = true;
				}
			}

			if (auto* alpha = rt.alphaProperty.get()) {
				const bool testing  = alpha->GetAlphaTesting();
				const bool blending = alpha->GetAlphaBlending();
				// Blended whenever it blends, as a beard drawn solid down to its cut shows its whole patch. One that's cut too has
				// its strong strands drawn solid, so they hide what's behind them, while its faint edges can't hide anything
				a_out.useAlpha   = testing || blending;
				a_out.blendAlpha = blending;
				a_out.split      = blending && testing;
				a_out.cutoff     = testing ? std::clamp(alpha->alphaThreshold / 255.0f, 0.005f, 0.95f) : 0.005f;
			}
			if (a_out.alpha < 0.995f)
				a_out.useAlpha = a_out.blendAlpha = true;
			// An effect shell always blends, faint wherever it's faint
			if (a_out.effect) {
				a_out.useAlpha = a_out.blendAlpha = true;
				a_out.split                       = false;
				a_out.cutoff                      = 0.003f;
			}
		}

		bool ExtractOne(ID3D11Device* a_device, ID3D11DeviceContext* a_context, const Part& a_part, Mesh& a_out)
		{
			auto& rt = a_part.geometry->GetGeometryRuntimeData();
			bool  ok = false;
			// A skinned shape's buffer holds it unposed, so its skin's CPU copy is used even when it has one
			if (auto* skin = rt.skinInstance.get(); skin && skin->skinPartition && skin->skinPartition->numPartitions > 0) {
				ok = FromSkinPartition(a_device, a_part, *skin, *skin->skinPartition, a_out);
			} else if (rt.rendererData) {
				ok = FromRendererData(a_device, a_context, a_part, a_out);
			}
			if (ok)
				ReadMaterial(a_part.geometry, a_out);
			return ok;
		}

		// A model is someone else's memory, so a bad one is skipped rather than taking the game down
		__declspec(noinline) bool ExtractOneSEH(ID3D11Device* a_device, ID3D11DeviceContext* a_context, const Part* a_part, Mesh* a_out)
		{
			__try {
				return ExtractOne(a_device, a_context, *a_part, *a_out);
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				return false;
			}
		}

		// ---- a face's tint, built from its layers ------------------------

		constexpr std::uint32_t kTintSize = 512;

		// A mask's strength per texel at the tint's size, kept for the next face as most share them. Greyscale masks keep it in
		// their colour, others in their alpha, so it's the colour when that varies and the alpha when it doesn't. A byte a texel, and
		// let go of whole once it holds a few races' worth, as the masks a preset menu walks through would otherwise all stay
		constexpr std::size_t kTintMasksKept = 64;  // a quarter of a megabyte each

		const std::vector<std::uint8_t>* TintMask(const std::string& a_path)
		{
			static std::unordered_map<std::string, std::vector<std::uint8_t>> cache;

			std::string path = a_path;
			std::ranges::replace(path, '/', '\\');
			if (_strnicmp(path.c_str(), "textures\\", 9) != 0)
				path = "textures\\" + path;
			std::string key = path;
			std::ranges::transform(key, key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (const auto it = cache.find(key); it != cache.end())
				return it->second.empty() ? nullptr : &it->second;

			if (cache.size() >= kTintMasksKept)
				cache.clear();
			auto&                        out = cache[key];
			RE::BSResourceNiBinaryStream stream(path);
			if (!stream.good() || !stream.stream) {
				logger::warn("RenderModel: no tint mask at {}", path);
				return nullptr;
			}
			std::vector<std::uint8_t> data(stream.stream->totalSize);
			stream.read(data.data(), static_cast<std::uint32_t>(data.size()));

			DirectX::ScratchImage loaded, plain, sized;
			if (FAILED(DirectX::LoadFromDDSMemory(data.data(), data.size(), DirectX::DDS_FLAGS_NONE, nullptr, loaded))) {
				logger::warn("RenderModel: couldn't read the tint mask {}", path);
				return nullptr;
			}
			// Without WIC, which needs COM, and the render thread hasn't started it
			constexpr auto filter = DirectX::TEX_FILTER_LINEAR | DirectX::TEX_FILTER_FORCE_NON_WIC;

			const DirectX::Image* image  = loaded.GetImage(0, 0, 0);
			const DXGI_FORMAT     format = loaded.GetMetadata().format;
			// Already in the layout read below, so only the others are converted, as converting to the same format is refused
			HRESULT hr = S_OK;
			if (DirectX::IsCompressed(format))
				hr = DirectX::Decompress(*image, DXGI_FORMAT_R8G8B8A8_UNORM, plain);
			else if (format != DXGI_FORMAT_R8G8B8A8_UNORM)
				hr = DirectX::Convert(*image, DXGI_FORMAT_R8G8B8A8_UNORM, filter, DirectX::TEX_THRESHOLD_DEFAULT, plain);
			if (SUCCEEDED(hr)) {
				if (plain.GetImageCount() > 0)
					image = plain.GetImage(0, 0, 0);
				if (image->width != kTintSize || image->height != kTintSize) {
					hr    = DirectX::Resize(*image, kTintSize, kTintSize, filter, sized);
					image = SUCCEEDED(hr) ? sized.GetImage(0, 0, 0) : nullptr;
				}
			}
			if (FAILED(hr) || !image) {
				logger::warn("RenderModel: couldn't convert the tint mask {} (format {}, error {:#010x})", path, static_cast<int>(format), static_cast<std::uint32_t>(hr));
				return nullptr;
			}

			// In its colour when that varies, as vanilla's greyscale masks have it: some carry an alpha too, a hard block round the
			// mask (MaleHeadDirt_03), which read as the mask drew a sharp-edged patch. In its alpha only when its colour is flat, as a
			// mask painted white and shaped by its alpha is
			std::uint8_t lowRed = 255, highRed = 0, lowestAlpha = 255;
			for (std::uint32_t y = 0; y < kTintSize; ++y) {
				const auto* row = image->pixels + y * image->rowPitch;
				for (std::uint32_t x = 0; x < kTintSize; ++x) {
					lowRed      = std::min(lowRed, row[x * 4]);
					highRed     = std::max(highRed, row[x * 4]);
					lowestAlpha = std::min(lowestAlpha, row[x * 4 + 3]);
				}
			}
			const bool inAlpha = highRed - lowRed < 8 && lowestAlpha < 250;

			out.resize(static_cast<std::size_t>(kTintSize) * kTintSize);
			for (std::uint32_t y = 0; y < kTintSize; ++y) {
				const auto* row = image->pixels + y * image->rowPitch;
				for (std::uint32_t x = 0; x < kTintSize; ++x)
					out[y * kTintSize + x] = row[x * 4 + (inAlpha ? 3 : 0)];
			}
			return &out;
		}

		// The layers laid in order over mid-grey, which FaceGen's tint leaves the face as it is, as the game's tint pass does
		ComPtr<ID3D11ShaderResourceView> BuildFaceTint(ID3D11Device* a_device, const std::vector<Request::TintLayer>& a_layers)
		{
			std::vector<std::array<float, 3>> texels(static_cast<std::size_t>(kTintSize) * kTintSize, { 0.5f, 0.5f, 0.5f });
			for (const auto& layer : a_layers) {
				const auto* mask = layer.colour[3] > 0.0f ? TintMask(layer.mask) : nullptr;
				if (!mask)
					continue;
				for (std::size_t i = 0; i < texels.size(); ++i) {
					const float a = (*mask)[i] * (1.0f / 255.0f) * layer.colour[3];
					for (int c = 0; c < 3; ++c)
						texels[i][c] += (layer.colour[c] - texels[i][c]) * a;
				}
			}

			std::vector<std::uint32_t> rgba(texels.size());
			for (std::size_t i = 0; i < texels.size(); ++i) {
				const auto byte = [&](int c) { return static_cast<std::uint32_t>(std::clamp(texels[i][c], 0.0f, 1.0f) * 255.0f + 0.5f); };
				rgba[i]         = byte(0) | (byte(1) << 8) | (byte(2) << 16) | 0xFF000000u;
			}

			D3D11_TEXTURE2D_DESC desc{};
			desc.Width            = kTintSize;
			desc.Height           = kTintSize;
			desc.MipLevels        = 1;
			desc.ArraySize        = 1;
			desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage            = D3D11_USAGE_IMMUTABLE;
			desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

			D3D11_SUBRESOURCE_DATA           init{ rgba.data(), kTintSize * 4, 0 };
			ComPtr<ID3D11Texture2D>          texture;
			ComPtr<ID3D11ShaderResourceView> srv;
			if (FAILED(a_device->CreateTexture2D(&desc, &init, texture.GetAddressOf())) || FAILED(a_device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf())))
				return nullptr;
			return srv;
		}

		// ---- the drawing -------------------------------------------------

		struct Params
		{
			DirectX::XMFLOAT4X4 viewProj;
			DirectX::XMFLOAT3   lightDir;
			float               ambient          = 0.36f;
			DirectX::XMFLOAT3   eyePos;
			float               alpha            = 1.0f;
			DirectX::XMFLOAT3   tint{ 1.0f, 1.0f, 1.0f };
			float               cutoff           = 0.0f;
			std::uint32_t       useTexture       = 0;
			std::uint32_t       useAlpha         = 0;
			std::uint32_t       useTint          = 0;
			std::uint32_t       useFaceTint      = 0;
			std::uint32_t       useDetail        = 0;
			std::uint32_t       useNormalMap     = 0;     // 1 model space, 2 tangent space
			float               specularStrength = 0.10f;
			float               specularPower    = 42.0f;
			DirectX::XMFLOAT3   emissive{};
			float               emissiveStrength = 0.0f;
			float               backdropLift     = 0.0f;
			std::uint32_t       twoSided         = 1;
			std::uint32_t       blended          = 0;
			float               ceiling          = 2.0f;  // alpha at or over this isn't drawn
			std::uint32_t       useGlow          = 0;
			std::uint32_t       useEnv           = 0;     // 1 tinted by its surface, 2 untinted
			std::uint32_t       useEnvMask       = 0;
			float               envScale         = 0.0f;
			std::uint32_t       useVertexAlpha   = 0;
			std::uint32_t       useEffect        = 0;
			std::uint32_t       usePalette       = 0;     // 1 its colour, 2 its alpha too
			std::uint32_t       useFalloff       = 0;
			DirectX::XMFLOAT4   turn[3]{};                // a model space normal map's rows into the model's space
			DirectX::XMFLOAT4   effectColour{ 1.0f, 1.0f, 1.0f, 1.0f };
			DirectX::XMFLOAT4   falloffData{ 1.0f, 0.0f, 1.0f, 1.0f };
			float               effectScale      = 1.0f;
			float               exposure         = 1.0f;
			float               saturation       = 1.0f;
			float               contrast         = 1.0f;
			DirectX::XMFLOAT4   uvTransform{ 0.0f, 0.0f, 1.0f, 1.0f };  // offset, then scale
		};
		static_assert(sizeof(Params) % 16 == 0);

		constexpr char kShader[] = R"(
cbuffer Params : register(b0)
{
    float4x4 ViewProj;
    float3   LightDir;   float Ambient;
    float3   EyePos;     float MaterialAlpha;
    float3   Tint;       float AlphaCutoff;
    uint     UseTexture; uint  UseAlpha; uint UseTint; uint UseFaceTint;
    uint     UseDetail;  uint  UseNormalMap; float SpecularStrength; float SpecularPower;
    float3   Emissive;   float EmissiveStrength;
    float    BackdropLift; uint TwoSided; uint Blended; float AlphaCeiling;
    uint     UseGlow;    uint UseEnv; uint UseEnvMask; float EnvScale;
    uint     UseVertexAlpha; uint UseEffect; uint UsePalette; uint UseFalloff;
    float4   Turn[3];
    float4   EffectColour;
    float4   FalloffData;
    float    EffectScale; float Exposure; float Saturation; float Contrast;
    float4   UVTransform;
};
Texture2D    Diffuse   : register(t0);
Texture2D    FaceTint  : register(t1);
Texture2D    Detail    : register(t2);
Texture2D    NormalMap : register(t3);
Texture2D    GlowMap   : register(t4);
TextureCube  EnvMap    : register(t5);
Texture2D    EnvMask   : register(t6);
Texture2D    Palette   : register(t7);
SamplerState Samp      : register(s0);

struct VS_IN    { float3 Position : POSITION; float2 UV : TEXCOORD0; float3 Normal : NORMAL; float4 Colour : COLOR; };
struct PLAIN_IN { float3 Position : POSITION; float2 UV : TEXCOORD0; float3 Normal : NORMAL; };
struct VS_OUT { float4 Position : SV_Position; float2 UV : TEXCOORD0; float3 Normal : TEXCOORD1; float3 World : TEXCOORD2; float4 Colour : COLOR; };

VS_OUT ModelVS(VS_IN i)
{
    VS_OUT o;
    o.Position = mul(float4(i.Position, 1.0), ViewProj);
    o.UV       = i.UV * UVTransform.zw + UVTransform.xy;
    o.Normal   = normalize(i.Normal);
    o.World    = i.Position;
    o.Colour   = i.Colour;
    return o;
}

VS_OUT PlainVS(PLAIN_IN i)
{
    VS_IN full;
    full.Position = i.Position;
    full.UV       = i.UV;
    full.Normal   = i.Normal;
    full.Colour   = 1.0;
    return ModelVS(full);
}

// An effect shader's shell, unlit, as the game draws one: its texture times its base colour and scale, or with a palette, the
// colour its texture's green picks along the row its base colour's red picks, and the alpha its texture's alpha picks along the row
// its base colour's alpha picks. Faded by its vertices and by how square on it is to the camera
float4 EffectPS(VS_OUT i)
{
    float4 t = UseTexture ? Diffuse.Sample(Samp, i.UV) : 1.0;
    float4 c = float4(EffectColour.rgb * EffectScale, EffectColour.a) * t;
    if (UsePalette) {
        // Kept off the edges, as the sampler wraps and a palette's two ends are its opposite colours
        c.rgb = EffectScale * Palette.Sample(Samp, clamp(float2(t.g, EffectColour.r), 0.004, 0.996)).rgb;
        c.a   = UsePalette == 2 ? Palette.Sample(Samp, clamp(float2(t.a, EffectColour.a), 0.004, 0.996)).a : c.a;
    }
    c *= i.Colour;
    if (UseFalloff) {
        float d  = abs(dot(normalize(i.Normal), normalize(EyePos - i.World)));
        float k  = abs(FalloffData.y - FalloffData.x) > 0.0001 ? saturate((d - FalloffData.x) / (FalloffData.y - FalloffData.x)) : 0.0;
        c.a     *= lerp(FalloffData.z, FalloffData.w, k);
    }
    float a = saturate(c.a * MaterialAlpha);
    if (a <= AlphaCutoff) discard;
    float3 o = c.rgb < 0.8 ? c.rgb : 0.8 + 0.2 * (1.0 - exp(-(c.rgb - 0.8) / 0.2));
    return float4(saturate(o), a);
}

// An eye's reflection, soft: the cube map averaged over a small cone and taken from a blurrier mip, so it reads as a wet sheen and
// a catchlight rather than laying the cube map's own detail over the iris
float3 SoftReflection(float3 r)
{
    float3 a   = normalize(cross(r, abs(r.z) < 0.9 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0)));
    float3 b   = cross(r, a);
    float3 sum = EnvMap.SampleBias(Samp, float3(r.x, r.z, r.y), 3.0).rgb;
    [unroll] for (int k = 0; k < 8; ++k) {
        float  t = k * 0.7853982;
        float3 d = normalize(r + (a * cos(t) + b * sin(t)) * 0.35);
        sum     += EnvMap.SampleBias(Samp, float3(d.x, d.z, d.y), 3.0).rgb;
    }
    return sum / 9.0;
}

// A tangent space normal map's normal, its frame taken from how the position and UVs change across the pixel, so the meshes need no
// tangents. As the game's lighting shader reads one: green along the UVs' v, and z rebuilt from x and y, as a two channel map has none
float3 TangentNormal(float3 g, float3 p, float2 uv, float3 m, float strength)
{
    float3 dp1  = ddx(p), dp2 = ddy(p);
    float2 duv1 = ddx(uv), duv2 = ddy(uv);
    float3 dp2p = cross(dp2, g), dp1p = cross(g, dp1);
    float3 t    = dp2p * duv1.x + dp1p * duv2.x;
    float3 b    = dp2p * duv1.y + dp1p * duv2.y;
    // Its sign by which way round the pixel's square lies on the surface, so it holds seen from either side and either handedness
    float  s    = dot(dp1, dp2p) < 0.0 ? -1.0 : 1.0;
    float  k    = s * rsqrt(max(max(dot(t, t), dot(b, b)), 1e-20));
    float2 xy   = (m.xy * 2.0 - 1.0) * strength;
    float3 n    = t * k * xy.x + b * k * xy.y + g * sqrt(saturate(1.0 - dot(xy, xy)));
    return dot(n, n) > 1e-8 ? normalize(n) : g;
}

float4 ModelPS(VS_OUT i, bool front : SV_IsFrontFace) : SV_Target
{
    if (UseEffect) return EffectPS(i);
    float4 c = UseTexture ? Diffuse.Sample(Samp, i.UV) : float4(0.62, 0.60, 0.56, 1.0);
    float  a = saturate(MaterialAlpha * (UseAlpha ? c.a : 1.0) * (UseVertexAlpha ? i.Colour.a : 1.0));
    if (a <= AlphaCutoff || a >= AlphaCeiling) discard;

    // FaceGen's own blend, a soft light of the tint over the face, scaled by the face's detail
    if (UseFaceTint) {
        float3 t = FaceTint.Sample(Samp, i.UV).rgb;
        float3 d = UseDetail ? 3.984375 * (Detail.Sample(Samp, i.UV).rgb + float3(0.00392156886, 0.0, 0.00392156886)) : 1.0;
        c.rgb    = saturate((c.rgb * c.rgb + 2.0 * t * c.rgb * (1.0 - c.rgb)) * d);
    }
    // Hair takes its tint where its vertices' green says, and everything else its vertex colour, as the game's hair shader does
    c.rgb *= UseTint ? lerp(1.0, Tint, i.Colour.g) : i.Colour.rgb;

    // A one-sided see-through shape's far side left out, as hair's cards and a beard's layers would show their backs through each
    // other. By its normals rather than its winding, which the meshes don't agree on, and a little past edge on, as normals blended
    // across a triangle dip below it at the silhouette. A solid shape's far side is hidden by its near side, so it's kept, as a stray
    // normal would cut a hole in it
    float3 view = normalize(EyePos - i.World);
    if ((Blended || UseAlpha) && !TwoSided && dot(i.Normal, view) < -0.15) discard;

    // A model space normal map, stored as XZY, unless it faces away from the camera where the mesh doesn't. Only a two-sided
    // shape's back is turned to face the camera, else the far side peeping past a silhouette is lit as if facing it
    // Which side is seen by the normals too, not the winding, which the meshes don't agree on: a two-sided hair layer wound the other
    // way was lit from behind, all its strands dark, so a two-sided ponytail base (evg's male one) drew darker than the one-sided
    // female base it matches
    float  side = !TwoSided || dot(i.Normal, view) >= 0.0 ? 1.0 : -1.0;
    float3 g    = normalize(i.Normal);
    float3 n    = g * side;
    float  shine = 1.0;
    if (UseNormalMap == 1) {
        float3 m  = NormalMap.Sample(Samp, i.UV).xzy * 2.0 - 1.0;
        float3 mn = normalize(float3(dot(Turn[0].xyz, m), dot(Turn[1].xyz, m), dot(Turn[2].xyz, m)));
        n         = dot(mn, view) < -0.2 ? n : mn;
    } else if (UseNormalMap == 2) {
        // Shaped on the mesh's own side, then turned with it, as a leaf's back is the front's relief seen from behind. Where it shines
        // is in its alpha, as the game's specular takes it
        float4 m  = NormalMap.Sample(Samp, i.UV);
        // Hair's a touch flatter, as each strand's map curls it right round and the soft light below only wraps so far
        float3 mn = TangentNormal(g, i.World, i.UV, m.xyz, UseTint ? 0.75 : 1.0) * side;
        n         = dot(mn, view) < -0.2 ? n : mn;
        shine     = m.a;
    }
    // A studio's key, fill, top, and rim, as Fitting Room's cards have always been lit
    float3 key   = normalize(LightDir);
    float3 fill  = normalize(float3(-key.x * 0.75, key.y * 0.25, key.z * 0.55 + 0.25));
    float3 rimD  = normalize(float3(0.46, -0.36, 0.72));
    float  keyL  = saturate(dot(n, key));
    // Hair takes the light round its strands, as the game's hair shader softens it, so a normal-mapped strand's far half isn't dark
    float  wrap  = UseTint ? saturate(dot(n, key) * 0.5 + 0.5) : saturate(dot(n, key) * 0.65 + 0.35);
    float  top   = saturate(n.z * 0.5 + 0.5);
    float  spec  = pow(saturate(dot(n, normalize(key + view))), max(SpecularPower, 8.0)) * SpecularStrength * shine * (0.35 + keyL * 0.65);
    float  rim   = pow(1.0 - saturate(dot(n, view)), 2.2) * 0.12 * saturate(dot(n, rimD) * 0.55 + 0.45);
    float  light = Ambient + wrap * 0.56 + saturate(dot(n, fill)) * 0.30 + top * 0.08 + rim;
    float3 lit   = c.rgb * light + spec;
    // A cube map reflected where its mask says (1 its red, 2 its alpha, a normal map's gloss), and lit as the surface is, as the
    // game's environment shader does. Tinted mostly by the surface's own colour and a little by its hue, as metal colours what it
    // reflects, so bronze or rusted steel doesn't turn to chrome, and dark metal reflects darkly. The world's +Z up is the cube's +Y
    if (UseEnv) {
        float3 r    = reflect(-view, n);
        float4 mask = UseEnvMask ? EnvMask.Sample(Samp, i.UV) : 1.0;
        float  m    = UseEnvMask == 2 ? mask.a : mask.r;
        float3 hue  = c.rgb / max(max(c.r, max(c.g, c.b)), 0.05);
        // An eye's cornea reflects untinted, softly and faintly, so the iris under it still reads. Metal's raised, as the studio's
        // cube maps are dim and a small card's shine was lost
        float3 tint = UseEnv == 2 ? 0.45 : lerp(c.rgb, hue, 0.3) * 1.7;
        float3 env  = UseEnv == 2 ? SoftReflection(r) : EnvMap.Sample(Samp, float3(r.x, r.z, r.y)).rgb;
        lit        += env * tint * m * EnvScale * saturate(light);
    }
    // A glow map lights only where it says, as the game's does, e.g. a Daedric blade's veins
    lit        += saturate(Emissive) * EmissiveStrength * (UseGlow ? GlowMap.Sample(Samp, i.UV).rgb : 1.0);
    // Highlights rolled off rather than clipped, so a pale tint like white hair keeps its strands
    float3 o = (lit * 0.96 + c.rgb * 0.04) * Exposure;
    o        = o < 0.8 ? o : 0.8 + 0.2 * (1.0 - exp(-(o - 0.8) / 0.2));
    // The caller's grade, richer colour and deeper shade, about the mid tones, as a studio print would be
    o        = lerp(dot(o, float3(0.2126, 0.7152, 0.0722)).xxx, o, Saturation);
    o        = (o - 0.45) * Contrast + 0.45;
    // Solid unless blended, as a cut shape's soft alpha would otherwise leave holes in the picture
    return float4(saturate(o), Blended ? a : 1.0);
}

struct BG_OUT { float4 Position : SV_Position; float2 UV : TEXCOORD0; };
BG_OUT BackdropVS(uint id : SV_VertexID)
{
    float2 p[3] = { float2(-1, -1), float2(-1, 3), float2(3, -1) };
    BG_OUT o;
    o.Position = float4(p[id], 0, 1);
    o.UV       = p[id] * float2(0.5, -0.5) + 0.5;
    return o;
}
float4 BackdropPS(BG_OUT i) : SV_Target
{
    float  vignette = smoothstep(1.28, 0.20, length((i.UV * 2.0 - 1.0) * float2(0.78, 1.08)));
    float3 c        = lerp(float3(0.066, 0.072, 0.086), float3(0.030, 0.033, 0.040), i.UV.y);
    c              += vignette * 0.035 + smoothstep(0.52, 1.0, i.UV.y) * 0.025;
    c              *= 0.90 + vignette * 0.10;
    return float4(saturate(c + BackdropLift), 1.0);
}
)";

		struct Pipeline
		{
			ComPtr<ID3D11VertexShader>      vs, plainVs, bgVs;
			ComPtr<ID3D11PixelShader>       ps, bgPs;
			ComPtr<ID3D11InputLayout>       layout, plainLayout;
			ComPtr<ID3D11Buffer>            params;
			ComPtr<ID3D11RasterizerState>   raster;
			ComPtr<ID3D11BlendState>        opaque, blend, additive;
			ComPtr<ID3D11DepthStencilState> depthWrite, depthTest, depthOff;
			ComPtr<ID3D11SamplerState>      sampler;
			bool                            failed = false;
		};
		Pipeline g_pipe;

		// One set of targets a size, kept for the next picture that size, as portraits and cards each come in their own
		struct Targets
		{
			ComPtr<ID3D11Texture2D>        colour, depth, staging;
			ComPtr<ID3D11RenderTargetView> rtv;
			ComPtr<ID3D11DepthStencilView> dsv;
			std::uint32_t                  size = 0;
		};
		constexpr std::size_t kTargetSizesKept = 3;
		std::vector<Targets>  g_targets;  // the most recently used last

		ComPtr<ID3DBlob> Compile(const char* a_entry, const char* a_profile)
		{
			ComPtr<ID3DBlob> code, errors;
			if (FAILED(::D3DCompile(kShader, sizeof(kShader) - 1, "FUCKModelRender", nullptr, nullptr, a_entry, a_profile, 0, 0, code.GetAddressOf(), errors.GetAddressOf()))) {
				logger::error("RenderModel: the {} shader didn't compile: {}", a_entry, errors ? static_cast<const char*>(errors->GetBufferPointer()) : "");
				return nullptr;
			}
			return code;
		}

		// Made once, and given up on for the session if it fails, so a broken setup doesn't retry every frame
		bool EnsurePipeline(ID3D11Device* a_device)
		{
			auto& p = g_pipe;
			if (p.vs || p.failed)
				return !p.failed;
			p.failed = true;

			auto vs = Compile("ModelVS", "vs_5_0"), plainVs = Compile("PlainVS", "vs_5_0"), ps = Compile("ModelPS", "ps_5_0");
			auto bgVs = Compile("BackdropVS", "vs_5_0"), bgPs = Compile("BackdropPS", "ps_5_0");
			if (!vs || !plainVs || !ps || !bgVs || !bgPs)
				return false;

			const D3D11_INPUT_ELEMENT_DESC layout[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			if (FAILED(a_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, p.vs.GetAddressOf())) ||
				FAILED(a_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, p.ps.GetAddressOf())) ||
				FAILED(a_device->CreateVertexShader(bgVs->GetBufferPointer(), bgVs->GetBufferSize(), nullptr, p.bgVs.GetAddressOf())) ||
				FAILED(a_device->CreatePixelShader(bgPs->GetBufferPointer(), bgPs->GetBufferSize(), nullptr, p.bgPs.GetAddressOf())) ||
				FAILED(a_device->CreateInputLayout(layout, 4, vs->GetBufferPointer(), vs->GetBufferSize(), p.layout.GetAddressOf())) ||
				FAILED(a_device->CreateVertexShader(plainVs->GetBufferPointer(), plainVs->GetBufferSize(), nullptr, p.plainVs.GetAddressOf())) ||
				FAILED(a_device->CreateInputLayout(layout, 3, plainVs->GetBufferPointer(), plainVs->GetBufferSize(), p.plainLayout.GetAddressOf())))
				return false;

			D3D11_BUFFER_DESC cb{ sizeof(Params), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE };
			if (FAILED(a_device->CreateBuffer(&cb, nullptr, p.params.GetAddressOf())))
				return false;

			// Both sides, as plenty of meshes are single-sided
			D3D11_RASTERIZER_DESC rs{ D3D11_FILL_SOLID, D3D11_CULL_NONE };
			rs.DepthClipEnable = TRUE;
			if (FAILED(a_device->CreateRasterizerState(&rs, p.raster.GetAddressOf())))
				return false;

			D3D11_BLEND_DESC bd{};
			bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			if (FAILED(a_device->CreateBlendState(&bd, p.opaque.GetAddressOf())))
				return false;
			auto& rt          = bd.RenderTarget[0];
			rt.BlendEnable    = TRUE;
			rt.SrcBlend       = D3D11_BLEND_SRC_ALPHA;
			rt.DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
			rt.BlendOp        = D3D11_BLEND_OP_ADD;
			rt.SrcBlendAlpha  = D3D11_BLEND_ONE;
			rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
			rt.BlendOpAlpha   = D3D11_BLEND_OP_ADD;
			if (FAILED(a_device->CreateBlendState(&bd, p.blend.GetAddressOf())))
				return false;
			rt.DestBlend      = D3D11_BLEND_ONE;
			rt.DestBlendAlpha = D3D11_BLEND_ONE;
			if (FAILED(a_device->CreateBlendState(&bd, p.additive.GetAddressOf())))
				return false;

			D3D11_DEPTH_STENCIL_DESC ds{ TRUE, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS_EQUAL };
			if (FAILED(a_device->CreateDepthStencilState(&ds, p.depthWrite.GetAddressOf())))
				return false;
			ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			if (FAILED(a_device->CreateDepthStencilState(&ds, p.depthTest.GetAddressOf())))
				return false;
			ds.DepthEnable = FALSE;
			if (FAILED(a_device->CreateDepthStencilState(&ds, p.depthOff.GetAddressOf())))
				return false;

			D3D11_SAMPLER_DESC sd{ D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP };
			sd.MaxLOD = D3D11_FLOAT32_MAX;
			if (FAILED(a_device->CreateSamplerState(&sd, p.sampler.GetAddressOf())))
				return false;

			p.failed = false;
			return true;
		}

		Targets* EnsureTargets(ID3D11Device* a_device, std::uint32_t a_size)
		{
			if (const auto it = std::ranges::find(g_targets, a_size, &Targets::size); it != g_targets.end()) {
				std::rotate(it, it + 1, g_targets.end());
				return &g_targets.back();
			}

			Targets t;
			D3D11_TEXTURE2D_DESC desc{ a_size, a_size, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET };
			if (FAILED(a_device->CreateTexture2D(&desc, nullptr, t.colour.GetAddressOf())) || FAILED(a_device->CreateRenderTargetView(t.colour.Get(), nullptr, t.rtv.GetAddressOf())))
				return nullptr;
			auto depth      = desc;
			depth.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
			depth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			if (FAILED(a_device->CreateTexture2D(&depth, nullptr, t.depth.GetAddressOf())) || FAILED(a_device->CreateDepthStencilView(t.depth.Get(), nullptr, t.dsv.GetAddressOf())))
				return nullptr;
			auto staging           = desc;
			staging.Usage          = D3D11_USAGE_STAGING;
			staging.BindFlags      = 0;
			staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			if (FAILED(a_device->CreateTexture2D(&staging, nullptr, t.staging.GetAddressOf())))
				return nullptr;
			t.size = a_size;

			if (g_targets.size() >= kTargetSizesKept)
				g_targets.erase(g_targets.begin());
			g_targets.push_back(std::move(t));
			return &g_targets.back();
		}

		// The game's pipeline as it was, put back after, every reference it handed over let go
		struct StateGuard
		{
			// Every target, as a caller mid-frame may have the game's several bound at once
			ID3D11RenderTargetView*   rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
			ID3D11DepthStencilView*   dsv = nullptr;
			D3D11_VIEWPORT            viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
			UINT                      viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
			ID3D11VertexShader*       vs            = nullptr;
			ID3D11PixelShader*        ps            = nullptr;
			ID3D11GeometryShader*     gs            = nullptr;
			ID3D11HullShader*         hs            = nullptr;
			ID3D11DomainShader*       ds            = nullptr;
			ID3D11InputLayout*        layout        = nullptr;
			D3D11_PRIMITIVE_TOPOLOGY  topology{};
			ID3D11RasterizerState*    raster = nullptr;
			ID3D11BlendState*         blend  = nullptr;
			float                     blendFactor[4]{};
			UINT                      sampleMask = 0;
			ID3D11DepthStencilState*  depth      = nullptr;
			UINT                      stencilRef = 0;
			ID3D11Buffer*             vb         = nullptr;
			UINT                      stride = 0, offset = 0;
			ID3D11Buffer*             ib = nullptr;
			DXGI_FORMAT               ibFormat{};
			UINT                      ibOffset = 0;
			ID3D11Buffer*             vsCb     = nullptr;
			ID3D11Buffer*             psCb     = nullptr;
			ID3D11SamplerState*       sampler  = nullptr;
			ID3D11ShaderResourceView* srvs[8]{};

			explicit StateGuard(ID3D11DeviceContext* a_ctx) :
				ctx(a_ctx)
			{
				ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, &dsv);
				ctx->RSGetViewports(&viewportCount, viewports);
				ctx->VSGetShader(&vs, nullptr, nullptr);
				ctx->PSGetShader(&ps, nullptr, nullptr);
				ctx->GSGetShader(&gs, nullptr, nullptr);
				ctx->HSGetShader(&hs, nullptr, nullptr);
				ctx->DSGetShader(&ds, nullptr, nullptr);
				ctx->IAGetInputLayout(&layout);
				ctx->IAGetPrimitiveTopology(&topology);
				ctx->RSGetState(&raster);
				ctx->OMGetBlendState(&blend, blendFactor, &sampleMask);
				ctx->OMGetDepthStencilState(&depth, &stencilRef);
				ctx->IAGetVertexBuffers(0, 1, &vb, &stride, &offset);
				ctx->IAGetIndexBuffer(&ib, &ibFormat, &ibOffset);
				ctx->VSGetConstantBuffers(0, 1, &vsCb);
				ctx->PSGetConstantBuffers(0, 1, &psCb);
				ctx->PSGetSamplers(0, 1, &sampler);
				ctx->PSGetShaderResources(0, 8, srvs);
			}

			~StateGuard()
			{
				ctx->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, dsv);
				ctx->RSSetViewports(viewportCount, viewports);
				ctx->VSSetShader(vs, nullptr, 0);
				ctx->PSSetShader(ps, nullptr, 0);
				ctx->GSSetShader(gs, nullptr, 0);
				ctx->HSSetShader(hs, nullptr, 0);
				ctx->DSSetShader(ds, nullptr, 0);
				ctx->IASetInputLayout(layout);
				ctx->IASetPrimitiveTopology(topology);
				ctx->RSSetState(raster);
				ctx->OMSetBlendState(blend, blendFactor, sampleMask);
				ctx->OMSetDepthStencilState(depth, stencilRef);
				ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
				ctx->IASetIndexBuffer(ib, ibFormat, ibOffset);
				ctx->VSSetConstantBuffers(0, 1, &vsCb);
				ctx->PSSetConstantBuffers(0, 1, &psCb);
				ctx->PSSetSamplers(0, 1, &sampler);
				ctx->PSSetShaderResources(0, 8, srvs);

				for (auto* r : rtvs) {
					if (r)
						r->Release();
				}
				IUnknown* held[] = { dsv, vs, ps, gs, hs, ds, layout, raster, blend, depth, vb, ib, vsCb, psCb, sampler, srvs[0], srvs[1], srvs[2], srvs[3], srvs[4], srvs[5], srvs[6], srvs[7] };
				for (auto* h : held) {
					if (h)
						h->Release();
				}
			}

			ID3D11DeviceContext* ctx;
		};

		// Front on, as an actor faces down +y, the whole model's bounds in view, or only the face's when asked and there is one
		void Frame(const std::vector<Mesh>& a_meshes, const Request& a_request, Params& a_out)
		{
			const bool faceOnly = a_request.frameFace && std::ranges::any_of(a_meshes, [](const Mesh& a_mesh) { return a_mesh.faceGen && a_mesh.hasBounds; });

			std::array<float, 3> min{ -10.0f, -10.0f, -10.0f }, max{ 10.0f, 10.0f, 10.0f };
			bool                 have = false;
			for (const auto& m : a_meshes) {
				if (!m.hasBounds || (faceOnly && !m.faceGen))
					continue;
				for (int i = 0; i < 3; ++i) {
					min[i] = have ? std::min(min[i], m.min[i]) : m.min[i];
					max[i] = have ? std::max(max[i], m.max[i]) : m.max[i];
				}
				have = true;
			}

			using namespace DirectX;
			const XMFLOAT3 centre{ (min[0] + max[0]) * 0.5f, (min[1] + max[1]) * 0.5f, (min[2] + max[2]) * 0.5f };
			const float    radius = 0.5f * std::sqrt((max[0] - min[0]) * (max[0] - min[0]) + (max[1] - min[1]) * (max[1] - min[1]) + (max[2] - min[2]) * (max[2] - min[2]));

			constexpr float FOV   = XMConvertToRadians(30.0f);
			const float     dist  = std::max(radius, 1.0f) * a_request.margin / std::sin(FOV * 0.5f);
			const float     yaw   = XMConvertToRadians(a_request.yaw);
			const float     pitch = XMConvertToRadians(a_request.pitch);
			const XMVECTOR  dir   = XMVectorSet(std::sin(yaw) * std::cos(pitch), std::cos(yaw) * std::cos(pitch), std::sin(pitch), 0.0f);
			const XMVECTOR  at    = XMLoadFloat3(&centre);
			const XMVECTOR  eye   = XMVectorMultiplyAdd(dir, XMVectorReplicate(dist), at);

			// Right-handed, as the game's world is, so the picture isn't mirrored
			const XMMATRIX view = XMMatrixLookAtRH(eye, at, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
			const XMMATRIX proj = XMMatrixPerspectiveFovRH(FOV, 1.0f, std::max(dist - radius * 2.0f, 0.5f), dist + radius * 2.0f);
			XMStoreFloat4x4(&a_out.viewProj, XMMatrixTranspose(view * proj));
			XMStoreFloat3(&a_out.eyePos, eye);

			// The key light up and to one side of the camera
			XMStoreFloat3(&a_out.lightDir, XMVector3Normalize(XMVectorAdd(dir, XMVectorSet(-0.45f * std::cos(yaw), 0.45f * std::sin(yaw), 0.6f, 0.0f))));
		}

		// At most 4 a side, and never past 4096 pixels across
		std::uint32_t Supersample(std::uint32_t a_pixels, std::uint32_t a_asked)
		{
			std::uint32_t ss = std::clamp(a_asked, 1u, 4u);
			while (ss > 1 && a_pixels * ss > 4096)
				--ss;
			return ss;
		}

		// The target, a_ss times the picture a side, scaled down into a_rgba. Its colour is averaged as it's drawn, its alpha laid over
		// a clear background with what it covers multiplied in, then divided back out, so faint edges keep their colour rather than
		// fading to black as they would read as straight alpha
		void Resolve(const std::uint8_t* a_src, std::uint32_t a_pitch, std::uint32_t a_pixels, std::uint32_t a_ss, std::uint8_t* a_rgba)
		{
			const std::uint32_t n = a_ss * a_ss;
			for (std::uint32_t y = 0; y < a_pixels; ++y) {
				for (std::uint32_t x = 0; x < a_pixels; ++x) {
					std::uint32_t sum[4]{};
					for (std::uint32_t sy = 0; sy < a_ss; ++sy) {
						const auto* row = a_src + static_cast<std::size_t>(y * a_ss + sy) * a_pitch + static_cast<std::size_t>(x * a_ss) * 4;
						for (std::uint32_t sx = 0; sx < a_ss * 4; ++sx)
							sum[sx & 3] += row[sx];
					}
					auto* out = a_rgba + (static_cast<std::size_t>(y) * a_pixels + x) * 4;
					out[3]    = static_cast<std::uint8_t>((sum[3] + n / 2) / n);
					for (int c = 0; c < 3; ++c)
						out[c] = sum[3] ? static_cast<std::uint8_t>(std::min<std::uint32_t>(255, (sum[c] * 255 + sum[3] / 2) / sum[3])) : 0;
				}
			}
		}

		// The meshes from the camera in a_base, onto the backdrop unless a_transparent, read back into a_rgba
		bool Draw(ID3D11Device* a_device, ID3D11DeviceContext* a_ctx, const std::vector<Mesh>& a_meshes, const Params& a_base, std::uint32_t a_pixels,
			std::uint32_t a_supersample, bool a_transparent, std::uint8_t* a_rgba)
		{
			auto&               p      = g_pipe;
			const std::uint32_t ss     = Supersample(a_pixels, a_supersample);
			const std::uint32_t size   = a_pixels * ss;
			Targets*            target = EnsurePipeline(a_device) ? EnsureTargets(a_device, size) : nullptr;
			if (!target)
				return false;

			const Params& base   = a_base;
			const auto    upload = [&](const Params& a_params) {
				D3D11_MAPPED_SUBRESOURCE mapped{};
				if (FAILED(a_ctx->Map(p.params.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
					return false;
				std::memcpy(mapped.pData, &a_params, sizeof(a_params));
				a_ctx->Unmap(p.params.Get(), 0);
				return true;
			};

			StateGuard guard(a_ctx);

			const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			a_ctx->ClearRenderTargetView(target->rtv.Get(), clear);
			a_ctx->ClearDepthStencilView(target->dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
			ID3D11RenderTargetView* rtv = target->rtv.Get();
			a_ctx->OMSetRenderTargets(1, &rtv, target->dsv.Get());
			a_ctx->RSSetState(p.raster.Get());
			const D3D11_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(size), static_cast<float>(size), 0.0f, 1.0f };
			a_ctx->RSSetViewports(1, &viewport);
			a_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			a_ctx->GSSetShader(nullptr, nullptr, 0);
			a_ctx->HSSetShader(nullptr, nullptr, 0);
			a_ctx->DSSetShader(nullptr, nullptr, 0);

			ID3D11Buffer* cb = p.params.Get();
			a_ctx->VSSetConstantBuffers(0, 1, &cb);
			a_ctx->PSSetConstantBuffers(0, 1, &cb);
			if (!a_transparent && upload(base)) {
				a_ctx->OMSetBlendState(p.opaque.Get(), nullptr, 0xFFFFFFFF);
				a_ctx->OMSetDepthStencilState(p.depthOff.Get(), 0);
				a_ctx->IASetInputLayout(nullptr);
				a_ctx->VSSetShader(p.bgVs.Get(), nullptr, 0);
				a_ctx->PSSetShader(p.bgPs.Get(), nullptr, 0);
				a_ctx->Draw(3, 0);
			}
			a_ctx->PSSetShader(p.ps.Get(), nullptr, 0);
			ID3D11SamplerState* sampler = p.sampler.Get();
			a_ctx->PSSetSamplers(0, 1, &sampler);

			// A split shape's strong strands in the solid pass and its faint edges in the blended one, divided here
			constexpr float kSplit   = 0.5f;
			const auto      drawMesh = [&](const Mesh& a_mesh, bool a_blendPass) {
				Params params           = base;
				params.useTexture       = a_mesh.diffuse ? 1u : 0u;
				params.useAlpha         = a_mesh.useAlpha ? 1u : 0u;
				params.useTint          = a_mesh.hasTint ? 1u : 0u;
				params.useFaceTint      = a_mesh.faceTint ? 1u : 0u;
				params.useDetail        = a_mesh.faceTint && a_mesh.detail ? 1u : 0u;
				params.useNormalMap     = a_mesh.normalMap ? (a_mesh.tangentNormals ? 2u : 1u) : 0u;
				params.useGlow          = a_mesh.glow ? 1u : 0u;
				params.useEnv           = a_mesh.envMap && a_mesh.envScale > 0.0f ? (a_mesh.envUntinted ? 2u : 1u) : 0u;
				params.uvTransform      = { a_mesh.uvTransform[0], a_mesh.uvTransform[1], a_mesh.uvTransform[2], a_mesh.uvTransform[3] };
				params.useEnvMask       = a_mesh.envMask ? (a_mesh.envMaskInAlpha ? 2u : 1u) : 0u;
				params.envScale         = a_mesh.envScale;
				params.alpha            = a_mesh.alpha;
				params.cutoff           = a_mesh.split && !a_blendPass ? std::max(a_mesh.cutoff, kSplit) : a_mesh.cutoff;
				params.ceiling          = a_mesh.split && a_blendPass ? kSplit : 2.0f;
				params.tint             = { a_mesh.tint[0], a_mesh.tint[1], a_mesh.tint[2] };
				params.specularStrength = a_mesh.specularStrength;
				params.specularPower    = a_mesh.specularPower;
				params.emissive         = { a_mesh.emissive[0], a_mesh.emissive[1], a_mesh.emissive[2] };
				params.emissiveStrength = a_mesh.emissiveStrength;
				params.twoSided         = a_mesh.twoSided ? 1u : 0u;
				params.useVertexAlpha   = a_mesh.vertexAlpha ? 1u : 0u;
				params.blended          = a_blendPass ? 1u : 0u;
				params.useEffect        = a_mesh.effect ? 1u : 0u;
				params.usePalette       = a_mesh.effect && a_mesh.palette ? (a_mesh.paletteAlpha ? 2u : 1u) : 0u;
				params.useFalloff       = a_mesh.effect && a_mesh.falloff ? 1u : 0u;
				params.effectColour     = { a_mesh.effectColour[0], a_mesh.effectColour[1], a_mesh.effectColour[2], a_mesh.effectColour[3] };
				params.effectScale      = a_mesh.effectScale;
				params.falloffData      = { a_mesh.falloffData[0], a_mesh.falloffData[1], a_mesh.falloffData[2], a_mesh.falloffData[3] };
				for (int r = 0; r < 3; ++r)
					params.turn[r] = { a_mesh.normalTurn[r * 3], a_mesh.normalTurn[r * 3 + 1], a_mesh.normalTurn[r * 3 + 2], 0.0f };
				if (!upload(params))
					return;

				// A caller's own vertices are the first 32 bytes of FUCK's, without the colour
				a_ctx->IASetInputLayout(a_mesh.plain ? p.plainLayout.Get() : p.layout.Get());
				a_ctx->VSSetShader(a_mesh.plain ? p.plainVs.Get() : p.vs.Get(), nullptr, 0);
				const UINT    stride = a_mesh.plain ? static_cast<UINT>(offsetof(Vertex, r)) : sizeof(Vertex), offset = 0;
				ID3D11Buffer* vb     = a_mesh.vertexBuffer.Get();
				a_ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
				a_ctx->IASetIndexBuffer(a_mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
				ID3D11ShaderResourceView* srvs[8] = { a_mesh.diffuse.Get(), a_mesh.faceTint.Get(), a_mesh.detail.Get(), a_mesh.normalMap.Get(), a_mesh.glow.Get(),
					a_mesh.envMap.Get(), a_mesh.envMask.Get(), a_mesh.palette.Get() };
				a_ctx->PSSetShaderResources(0, 8, srvs);
				a_ctx->DrawIndexed(a_mesh.indexCount, 0, 0);
			};

			// Solid first, then what blends, over them
			a_ctx->OMSetDepthStencilState(p.depthWrite.Get(), 0);
			a_ctx->OMSetBlendState(p.opaque.Get(), nullptr, 0xFFFFFFFF);
			for (const auto& mesh : a_meshes) {
				if (!mesh.blendAlpha || mesh.split)
					drawMesh(mesh, false);
			}
			// What blends laid on farthest first, as the game sorts it, by the middle of its bounds. One without bounds, a caller's own,
			// keeps its place in the order given
			std::vector<std::pair<float, const Mesh*>> blended;
			for (const auto& mesh : a_meshes) {
				if (!mesh.blendAlpha)
					continue;
				float away = 0.0f;
				if (mesh.hasBounds) {
					const float d[3] = { (mesh.min[0] + mesh.max[0]) * 0.5f - base.eyePos.x, (mesh.min[1] + mesh.max[1]) * 0.5f - base.eyePos.y,
						(mesh.min[2] + mesh.max[2]) * 0.5f - base.eyePos.z };
					away = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
				}
				blended.emplace_back(away, &mesh);
			}
			if (std::ranges::all_of(blended, [](const auto& a_b) { return a_b.second->hasBounds; }))
				std::ranges::stable_sort(blended, std::ranges::greater{}, &std::pair<float, const Mesh*>::first);

			a_ctx->OMSetDepthStencilState(p.depthTest.Get(), 0);
			for (const auto& [away, mesh] : blended) {
				// An additive shell, as many effect shaders are, adds its light to what's behind it
				a_ctx->OMSetBlendState(mesh->additive ? p.additive.Get() : p.blend.Get(), nullptr, 0xFFFFFFFF);
				drawMesh(*mesh, true);
			}

			ID3D11ShaderResourceView* none[8]{};
			a_ctx->PSSetShaderResources(0, 8, none);

			// Read back at once, which waits on the GPU. Fine for the odd picture
			a_ctx->CopyResource(target->staging.Get(), target->colour.Get());
			D3D11_MAPPED_SUBRESOURCE mapped{};
			if (FAILED(a_ctx->Map(target->staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
				return false;
			Resolve(static_cast<const std::uint8_t*>(mapped.pData), mapped.RowPitch, a_pixels, ss, a_rgba);
			a_ctx->Unmap(target->staging.Get(), 0);
			return true;
		}

		void Finish(const Request& a_request, bool a_success)
		{
			if (!a_success)
				logger::warn("RenderModel: couldn't make {}", a_request.pathUtf8);
			if (a_request.callback)
				a_request.callback(a_request.pathUtf8.c_str(), a_success, a_request.userdata);
		}

		// Encoded off the render thread, as a .png takes a while, one after another on a thread of their own, so a menu's worth of
		// portraits doesn't start a thread each. Never let go of, as it may still be writing when the game closes
		struct Writer
		{
			struct Job
			{
				Request                   request;
				std::vector<std::uint8_t> rgba;
			};

			std::mutex              lock;
			std::condition_variable wake;
			std::deque<Job>         jobs;
			bool                    started = false;

			[[noreturn]] void Run()
			{
				// For WIC, which the .png is encoded by. Left started, as the thread runs till the game closes
				(void)::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
				for (;;) {
					Job job;
					{
						std::unique_lock held(lock);
						wake.wait(held, [&] { return !jobs.empty(); });
						job = std::move(jobs.front());
						jobs.pop_front();
					}
					const auto&    request = job.request;
					DirectX::Image image{ request.pixels, request.pixels, DXGI_FORMAT_R8G8B8A8_UNORM, request.pixels * 4u, request.pixels * request.pixels * 4u,
						job.rgba.data() };

					std::error_code             ec;
					const std::filesystem::path path{ request.path };
					std::filesystem::create_directories(path.parent_path(), ec);
					const bool ok = SUCCEEDED(DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path.c_str()));
					Finish(request, ok);
				}
			}
		};

		void Write(Request a_request, std::vector<std::uint8_t> a_rgba)
		{
			static auto* writer = new Writer;
			a_request.model.reset();
			{
				std::scoped_lock held(writer->lock);
				writer->jobs.push_back({ std::move(a_request), std::move(a_rgba) });
				if (!std::exchange(writer->started, true))
					std::thread([] { writer->Run(); }).detach();
			}
			writer->wake.notify_one();
		}

		std::mutex          g_lock;
		std::deque<Request> g_pending;

		// The thread FUCK draws on, which DrawMeshes is taken on alone, as it draws with the game's own context
		std::atomic<DWORD> g_renderThread{ 0 };

		// Pictures drawn a frame, taken one at a time until this long has gone, as each waits on the GPU. One is always taken
		constexpr auto kFrameBudget = std::chrono::milliseconds(4);
	}

	void Queue(Request a_request)
	{
		std::scoped_lock lock(g_lock);
		g_pending.push_back(std::move(a_request));
	}

	void OnFrame(ID3D11Device* a_device, ID3D11DeviceContext* a_context)
	{
		g_renderThread.store(::GetCurrentThreadId(), std::memory_order_relaxed);

		const auto start = std::chrono::steady_clock::now();
		for (bool first = true; first || std::chrono::steady_clock::now() - start < kFrameBudget; first = false) {
			Request request;
			{
				std::scoped_lock lock(g_lock);
				if (g_pending.empty())
					return;
				request = std::move(g_pending.front());
				g_pending.pop_front();
			}

			std::vector<Part> parts;
			Collect(request.model.get(), request.model->world.Invert(), parts);

			std::vector<Mesh> meshes;
			for (const auto& part : parts) {
				Mesh mesh;
				if (!ExtractOneSEH(a_device, a_context, &part, &mesh))
					continue;
				// A see-through layer whose texture isn't there, which would draw as a solid patch of its tint, e.g. a beard over the
				// jaw, so it's left out: its texture is what shapes it
				if (mesh.useAlpha && !mesh.diffuse && !mesh.effect)
					continue;
				meshes.push_back(std::move(mesh));
			}
			// The face's tint from the layers asked for, in place of the game's
			if (!request.faceTint.empty()) {
				if (auto tint = BuildFaceTint(a_device, request.faceTint)) {
					for (auto& mesh : meshes) {
						if (mesh.faceGen)
							mesh.faceTint = tint;
					}
				}
			}

			Params camera{};
			Frame(meshes, request, camera);
			// A portrait lit brighter than the studio's even light, as a face read darker beside the same face in game: more of the
			// soft fill, which lifts the face's shade without raising its highlights, a little more exposure, and a touch more colour,
			// as a beard's tint read muted
			camera.ambient    = 0.48f;
			camera.exposure   = 1.2f;
			camera.saturation = 1.1f;
			std::vector<std::uint8_t> rgba(static_cast<std::size_t>(request.pixels) * request.pixels * 4);
			if (meshes.empty() || !Draw(a_device, a_context, meshes, camera, request.pixels, request.supersample, request.transparent, rgba.data())) {
				Finish(request, false);
				continue;
			}
			Write(std::move(request), std::move(rgba));
		}
	}

	namespace
	{
		// A MeshDraw's look back to as it comes, so nothing a shape read before it is left on it. What isn't its look kept: its size,
		// its buffers, and normalTurn, which only the caller knows
		void ResetLook(FUCK::MeshDraw& a_out)
		{
			FUCK::MeshDraw fresh;
			fresh.size          = a_out.size;
			fresh.vertexBuffer  = a_out.vertexBuffer;
			fresh.vertexColours = a_out.vertexColours;
			fresh.indexBuffer   = a_out.indexBuffer;
			fresh.indexCount    = a_out.indexCount;
			std::ranges::copy(a_out.normalTurn, fresh.normalTurn);
			a_out = fresh;
		}

		// The material's first texture transform, which every texture it draws takes
		void ReadUVs(const RE::BSShaderMaterial* a_material, FUCK::MeshDraw& a_out)
		{
			const auto finite = [](float a_v, float a_fallback) { return std::isfinite(a_v) ? std::clamp(a_v, -1024.0f, 1024.0f) : a_fallback; };
			a_out.uvOffset[0] = finite(a_material->texCoordOffset[0].x, 0.0f);
			a_out.uvOffset[1] = finite(a_material->texCoordOffset[0].y, 0.0f);
			a_out.uvScale[0]  = finite(a_material->texCoordScale[0].x, 1.0f);
			a_out.uvScale[1]  = finite(a_material->texCoordScale[0].y, 1.0f);
			// A scale of nothing samples one texel across the whole shape, a flat patch of it, as a material built at run time can
			// leave it unset, so it's taken as no transform
			if (std::abs(a_out.uvScale[0]) < 1e-4f || std::abs(a_out.uvScale[1]) < 1e-4f) {
				a_out.uvOffset[0] = a_out.uvOffset[1] = 0.0f;
				a_out.uvScale[0] = a_out.uvScale[1] = 1.0f;
			}
		}

		// An effect shader's shell, e.g. a Ghostblade's or a conjured weapon's, read as the game's effect shader draws it: its texture
		// times its base colour, or a colour from its palette, faded at the angle its falloff gives, and usually added to what's behind
		bool ReadEffectMaterial(RE::BSEffectShaderProperty* a_effect, RE::NiProperty* a_alpha, FUCK::MeshDraw& a_out)
		{
			if (!a_effect->material)
				return false;
			const auto finite = [](float a_v, float a_min, float a_max, float a_fallback) {
				return std::isfinite(a_v) ? std::clamp(a_v, a_min, a_max) : a_fallback;
			};
			using Flag = RE::BSShaderProperty::EShaderPropertyFlag;
			// By bit, as the game numbers them: greyscale to palette colour, to palette alpha, and falloff
			const auto has = [&](int a_bit) { return a_effect->flags.any(static_cast<Flag>(1ull << a_bit)); };

			auto* material = static_cast<RE::BSEffectShaderMaterial*>(a_effect->material);
			ResetLook(a_out);
			ReadUVs(material, a_out);
			a_out.effect          = true;
			a_out.diffuse         = TextureSrv(material->sourceTexture.get());
			a_out.effectColour[0] = finite(material->baseColor.red, 0.0f, 1.0f, 1.0f);
			a_out.effectColour[1] = finite(material->baseColor.green, 0.0f, 1.0f, 1.0f);
			a_out.effectColour[2] = finite(material->baseColor.blue, 0.0f, 1.0f, 1.0f);
			a_out.effectColour[3] = finite(material->baseColor.alpha, 0.0f, 1.0f, 1.0f);
			a_out.effectScale     = finite(material->baseColorScale, 0.0f, 8.0f, 1.0f);
			if (has(4)) {
				a_out.palette      = TextureSrv(material->greyscaleTexture.get());
				a_out.paletteAlpha = a_out.palette && has(5);
			}
			if (has(6)) {
				a_out.falloff           = true;
				a_out.falloffAngles[0]  = finite(material->falloffStartAngle, -1.0f, 1.0f, 1.0f);
				a_out.falloffAngles[1]  = finite(material->falloffStopAngle, -1.0f, 1.0f, 0.0f);
				a_out.falloffOpacity[0] = finite(material->falloffStartOpacity, 0.0f, 1.0f, 1.0f);
				a_out.falloffOpacity[1] = finite(material->falloffStopOpacity, 0.0f, 1.0f, 1.0f);
			}
			a_out.alpha      = finite(a_effect->alpha, 0.0f, 1.0f, 1.0f);
			a_out.vertexAlpha = true;
			a_out.twoSided   = a_effect->flags.any(Flag::kTwoSided);
			a_out.useAlpha   = true;
			a_out.blendAlpha = true;
			a_out.cutoff     = 0.003f;
			// Added when it blends onto what's behind it at full, the NIF format's ONE (0) as its far side's factor
			if (auto* alpha = netimmerse_cast<RE::NiAlphaProperty*>(a_alpha))
				a_out.additive = alpha->GetAlphaBlending() && static_cast<int>(alpha->GetDestBlendMode()) == 0;
			return true;
		}
	}

	bool ReadMeshMaterial(RE::NiProperty* a_shader, RE::NiProperty* a_alpha, FUCK::MeshDraw& a_out)
	{
		// A material's value clamped, and its default where it's NaN or infinite, as a broken NIF can carry either
		const auto finite = [](float a_v, float a_min, float a_max, float a_fallback) {
			return std::isfinite(a_v) ? std::clamp(a_v, a_min, a_max) : a_fallback;
		};
		using Flag = RE::BSShaderProperty::EShaderPropertyFlag;

		if (auto* effect = netimmerse_cast<RE::BSEffectShaderProperty*>(a_shader))
			return ReadEffectMaterial(effect, a_alpha, a_out);

		auto* lighting = netimmerse_cast<RE::BSLightingShaderProperty*>(a_shader);
		if (!lighting || !lighting->material)
			return false;
		auto* material = static_cast<RE::BSLightingShaderMaterialBase*>(lighting->material);
		ResetLook(a_out);
		ReadUVs(material, a_out);

		a_out.diffuse             = TextureSrv(material->diffuseTexture.get());
		a_out.normalMap           = TextureSrv(material->normalTexture.get());
		a_out.normalMapModelSpace = lighting->flags.any(Flag::kModelSpaceNormals);
		a_out.alpha   = std::clamp(finite(lighting->alpha, 0.0f, 1.0f, 1.0f) * finite(material->materialAlpha, 0.0f, 1.0f, 1.0f), 0.0f, 1.0f);

		// Faded by its material's alpha, a screen door, premultiplied alpha, or its vertices, and so drawn over the solid meshes
		a_out.vertexAlpha = lighting->flags.any(Flag::kVertexAlpha);
		const bool fades  = a_out.alpha < 0.995f || lighting->flags.any(Flag::kScreendoorAlphaFade) ||
		                   lighting->flags.any(Flag::kPremultAlpha) || a_out.vertexAlpha;
		a_out.useAlpha    = fades;
		a_out.blendAlpha  = fades;

		if (lighting->flags.any(Flag::kSpecular)) {
			const float scale        = finite(material->specularColorScale, 0.0f, 2.5f, 1.0f);
			a_out.specularStrength = std::clamp(0.04f + scale * 0.055f, 0.02f, 0.18f);
		} else {
			a_out.specularStrength = 0.045f;
		}
		a_out.specularPower = finite(material->specularPower, 10.0f, 96.0f, 42.0f);

		// One sided unless its shader says otherwise, as the game culls its back, so a blended layer like a beard's isn't laid on twice
		a_out.twoSided = lighting->flags.any(Flag::kTwoSided);

		// A faint self light where it emits, e.g. a lantern's glass
		const bool emits = lighting->emissiveColor &&
		                   (lighting->flags.any(Flag::kOwnEmit) || lighting->flags.any(Flag::kGlowMap) || lighting->emissiveMult > 0.01f);
		if (emits) {
			a_out.emissive[0]      = finite(lighting->emissiveColor->red, 0.0f, 1.0f, 0.0f);
			a_out.emissive[1]      = finite(lighting->emissiveColor->green, 0.0f, 1.0f, 0.0f);
			a_out.emissive[2]      = finite(lighting->emissiveColor->blue, 0.0f, 1.0f, 0.0f);
			a_out.emissiveStrength = finite(lighting->emissiveMult * 0.12f, 0.0f, 0.32f, 0.0f);
		}

		switch (material->GetFeature()) {
		case RE::BSShaderMaterial::Feature::kHairTint:
			{
				// Hair and beards take the actor's hair colour from their material, as the game's hair shader does
				const auto& c = static_cast<RE::BSLightingShaderMaterialHairTint*>(material)->tintColor;
				a_out.useTint = true;
				a_out.tint[0] = finite(c.red, 0.0f, 1.0f, 1.0f);
				a_out.tint[1] = finite(c.green, 0.0f, 1.0f, 1.0f);
				a_out.tint[2] = finite(c.blue, 0.0f, 1.0f, 1.0f);
				break;
			}
		case RE::BSShaderMaterial::Feature::kEnvironmentMap:
			{
				// A cube map reflected where its mask says, so metal shines as it does in game. With no mask, the normal map's
				// gloss in its alpha, as the game's environment shader takes it
				auto* env      = static_cast<RE::BSLightingShaderMaterialEnvmap*>(material);
				a_out.envMap   = TextureSrv(env->envTexture.get());
				a_out.envMask  = TextureSrv(env->envMaskTexture.get());
				a_out.envScale = finite(env->envMapScale, 0.0f, 4.0f, 0.0f);
				if (!a_out.envMask) {
					a_out.envMask        = TextureSrv(material->normalTexture.get());
					a_out.envMaskInAlpha = a_out.envMask != nullptr;
				}
				break;
			}
		case RE::BSShaderMaterial::Feature::kEye:
			{
				// An eye's cornea reflects its cube map where its mask says, untinted by the iris under it, as the game's eye shader does
				auto* eye         = static_cast<RE::BSLightingShaderMaterialEye*>(material);
				a_out.envMap      = TextureSrv(eye->envTexture.get());
				a_out.envMask     = TextureSrv(eye->envMaskTexture.get());
				a_out.envScale    = finite(eye->envMapScale, 0.0f, 4.0f, 0.0f);
				a_out.envUntinted = true;
				break;
			}
		case RE::BSShaderMaterial::Feature::kGlowMap:
			// A glow map lights only its own marks, so it takes the full strength the game gives it, e.g. a Daedric blade's veins
			if (emits) {
				a_out.glow = TextureSrv(static_cast<RE::BSLightingShaderMaterialGlowmap*>(material)->glowTexture.get());
				if (a_out.glow)
					a_out.emissiveStrength = finite(lighting->emissiveMult, 0.0f, 2.0f, 0.0f);
			}
			break;
		default:
			break;
		}

		if (auto* alpha = netimmerse_cast<RE::NiAlphaProperty*>(a_alpha)) {
			const bool testing  = alpha->GetAlphaTesting();
			const bool blending = alpha->GetAlphaBlending();
			// Over the solid meshes when it blends as the pass can, source alpha over its inverse. One blended another way but cut
			// too is drawn solid, its cut shaping it
			// 6 and 7 in the NIF format's own order
			const bool plainBlend = static_cast<int>(alpha->GetSrcBlendMode()) == 6 && static_cast<int>(alpha->GetDestBlendMode()) == 7;
			a_out.useAlpha   = a_out.useAlpha || testing || blending;
			a_out.blendAlpha = a_out.blendAlpha || (blending && (plainBlend || !testing));
			if (testing)
				a_out.cutoff = std::clamp(alpha->alphaThreshold > 0 ? alpha->alphaThreshold / 255.0f : 0.04f, 0.005f, 0.95f);
			else if (blending)
				a_out.cutoff = 0.005f;
		}
		return true;
	}

	bool DrawMeshes(const FUCK::MeshDrawRequest& a_request)
	{
		// With the game's own context, so off the thread FUCK draws on it would race the game's drawing
		if (const auto thread = g_renderThread.load(std::memory_order_relaxed); thread != 0 && thread != ::GetCurrentThreadId()) {
			static std::once_flag said;
			std::call_once(said, [] { logger::warn("DrawMeshes: called off the render thread, so refused"); });
			return false;
		}

		auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
		auto* device   = renderer ? reinterpret_cast<ID3D11Device*>(renderer->GetRuntimeData().forwarder) : nullptr;
		auto* context  = renderer ? reinterpret_cast<ID3D11DeviceContext*>(renderer->GetRuntimeData().context) : nullptr;
		if (!device || !context || !a_request.meshes || a_request.meshCount == 0 || !a_request.rgba || a_request.pixels == 0 || a_request.pixels > 4096)
			return false;

		// Stepped through by the caller's own size, so a later MeshDraw's options don't throw an older caller's array out. A size this
		// version never had isn't a MeshDraw it knows, e.g. one built before MeshDraw carried its size
		const std::uint32_t stride = a_request.meshes[0].size;
		if (stride != sizeof(FUCK::MeshDraw)) {
			static std::once_flag said;
			std::call_once(said, [stride] { logger::warn("DrawMeshes: a MeshDraw of {} bytes isn't one this FUCK knows, so refused", stride); });
			return false;
		}
		const auto* bytes = reinterpret_cast<const std::uint8_t*>(a_request.meshes);

		std::vector<Mesh> meshes;
		meshes.reserve(a_request.meshCount);
		for (std::uint32_t i = 0; i < a_request.meshCount; ++i) {
			FUCK::MeshDraw in;
			std::memcpy(&in, bytes + static_cast<std::size_t>(i) * stride, std::min<std::size_t>(stride, sizeof(in)));
			if (!in.vertexBuffer || !in.indexBuffer || in.indexCount == 0)
				continue;
			Mesh mesh;
			mesh.vertexBuffer     = static_cast<ID3D11Buffer*>(in.vertexBuffer);
			mesh.indexBuffer      = static_cast<ID3D11Buffer*>(in.indexBuffer);
			mesh.diffuse          = static_cast<ID3D11ShaderResourceView*>(in.diffuse);
			mesh.indexCount       = in.indexCount;
			mesh.plain            = !in.vertexColours;
			mesh.vertexAlpha      = in.vertexColours && in.vertexAlpha;
			mesh.useAlpha         = in.useAlpha;
			mesh.blendAlpha       = in.blendAlpha;
			mesh.alpha            = in.alpha;
			mesh.cutoff           = in.cutoff;
			mesh.specularStrength = in.specularStrength;
			mesh.specularPower    = in.specularPower;
			mesh.emissive         = { in.emissive[0], in.emissive[1], in.emissive[2] };
			mesh.emissiveStrength = in.emissiveStrength;
			mesh.hasTint          = in.useTint;
			mesh.tint             = { in.tint[0], in.tint[1], in.tint[2] };
			mesh.twoSided         = in.twoSided;
			mesh.glow             = static_cast<ID3D11ShaderResourceView*>(in.glow);
			mesh.envMap           = static_cast<ID3D11ShaderResourceView*>(in.envMap);
			mesh.envMask          = static_cast<ID3D11ShaderResourceView*>(in.envMask);
			mesh.envMaskInAlpha   = in.envMaskInAlpha;
			mesh.envScale         = in.envScale;
			mesh.envUntinted      = in.envUntinted;
			mesh.normalMap        = static_cast<ID3D11ShaderResourceView*>(in.normalMap);
			mesh.tangentNormals   = !in.normalMapModelSpace;
			std::ranges::copy(in.normalTurn, mesh.normalTurn.begin());
			mesh.uvTransform      = { in.uvOffset[0], in.uvOffset[1], in.uvScale[0], in.uvScale[1] };
			CopyEffect(in, mesh);
			meshes.push_back(std::move(mesh));
		}
		if (meshes.empty())
			return false;

		Params camera{};
		std::memcpy(&camera.viewProj, a_request.viewProj, sizeof(camera.viewProj));
		DirectX::XMStoreFloat4x4(&camera.viewProj, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&camera.viewProj)));
		camera.eyePos       = { a_request.eye[0], a_request.eye[1], a_request.eye[2] };
		camera.lightDir     = { a_request.lightDir[0], a_request.lightDir[1], a_request.lightDir[2] };
		camera.backdropLift = a_request.backdropLift;
		camera.exposure     = std::isfinite(a_request.exposure) ? std::clamp(a_request.exposure, 0.25f, 4.0f) : 1.0f;
		camera.saturation   = std::isfinite(a_request.saturation) ? std::clamp(a_request.saturation, 0.0f, 3.0f) : 1.0f;
		camera.contrast     = std::isfinite(a_request.contrast) ? std::clamp(a_request.contrast, 0.25f, 3.0f) : 1.0f;
		return Draw(device, context, meshes, camera, a_request.pixels, a_request.supersample, a_request.transparent, a_request.rgba);
	}
}
