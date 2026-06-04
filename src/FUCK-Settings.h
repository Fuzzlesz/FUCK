#pragma once

class SettingsTool : public FUCK::ITool
{
public:
	const char* Name() const override;
	void        Draw() override;
	bool        OnAsyncInput(const void* a_event) override;
	void        OnClose() override;
};
