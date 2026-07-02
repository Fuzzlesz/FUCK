#include "FUCK-Man.h"
#include "ImGui/Audio.h"

#include "Input.h"
#include "Journal.h"

namespace Journal
{
	static bool s_fuckButtonInjected = false;

	constexpr const char* kSystemPagePath = "_root.QuestJournalFader.Menu_mc.SystemFader.Page_mc";

	static void TryInjectFUCKButton(RE::GFxMovieView* a_movieView)
	{
		if (!a_movieView || s_fuckButtonInjected)
			return;

		auto* manager = FUCKMan::GetSingleton();

		RE::GFxValue page, cat, listObj, entryList;
		if (!a_movieView->GetVariable(&page, kSystemPagePath) || !page.IsObject()      ||
			!page.GetMember("CategoryList_mc", &cat)          || !cat.IsObject()       ||
			!cat.GetMember("List_mc", &listObj)               || !listObj.IsObject()   ||
			!listObj.GetMember("entryList", &entryList)       || !entryList.IsArray())  {
			return;
		}

		const std::uint32_t arraySize = entryList.GetArraySize();
		if (arraySize == 0)
			return;

		const char* menuName = ("$FUCK_Title"_T);

		// Determine menu version
		bool isSafeMenu = page.HasMember("UpdateIndices");
		manager->SetJournalMenuType(isSafeMenu ? FUCKMan::JournalMenuType::kSafe : FUCKMan::JournalMenuType::kSkyUIv5);

		bool          replaced = false;
		std::uint32_t quitIdx  = arraySize;

		// 1. Check for duplicates, locate $QUIT, and optionally replace $HELP
		for (std::uint32_t i = 0; i < arraySize; ++i) {
			RE::GFxValue element, textVal;
			if (entryList.GetElement(i, &element) && element.IsObject() && element.GetMember("text", &textVal) && textVal.IsString()) {
				std::string_view textStr(textVal.GetString());

				if (textStr == menuName) {
					s_fuckButtonInjected = true;
					return;
				}

				if (textStr == "$QUIT") {
					quitIdx = i;
				}

				// Only replace $HELP if the user enabled it AND we are on SkyUI v5 or Vanilla
				if (!isSafeMenu && manager->GetReplaceHelpMenu() && textStr == "$HELP") {
					RE::GFxValue newEntry;
					a_movieView->CreateObject(&newEntry);
					newEntry.SetMember("text", menuName);
					entryList.SetElement(i, newEntry);
					replaced = true;
				}
			}
		}

		// 2. If we didn't replace $HELP, inject our new entry
		if (!replaced) {
			RE::GFxValue newEntry;
			a_movieView->CreateObject(&newEntry);
			newEntry.SetMember("text", menuName);

			if (isSafeMenu && quitIdx < arraySize) {
				// Push the last element back to safely grow the array size by 1 in GFx
				RE::GFxValue lastTemp;
				entryList.GetElement(arraySize - 1, &lastTemp);
				entryList.PushBack(lastTemp);

				// Shift elements down by 1 to make room at quitIdx
				for (std::uint32_t i = arraySize - 1; i > quitIdx; --i) {
					RE::GFxValue temp;
					entryList.GetElement(i - 1, &temp);
					entryList.SetElement(i, temp);
				}
				// Insert our new entry above $QUIT
				entryList.SetElement(quitIdx, newEntry);

				// Re-align internal indices for SkyUI v6+
				page.Invoke("UpdateIndices", nullptr, nullptr, 0);
			} else {
				// Fallback: append to the very bottom for SkyUI v5 so we don't shift hardcoded indices
				entryList.PushBack(newEntry);
			}
		}

		// 3. Invalidate lets us actually select the new entry
		listObj.Invoke("InvalidateData", nullptr, nullptr, 0);

		s_fuckButtonInjected = true;
	}

	static void TryRemoveFUCKButton(RE::GFxMovieView* a_movieView)
	{
		if (!a_movieView || !s_fuckButtonInjected)
			return;

		auto*        manager = FUCKMan::GetSingleton();
		RE::GFxValue page, cat, listObj, entryList;
		if (!a_movieView->GetVariable(&page, kSystemPagePath) || !page.IsObject() ||
			!page.GetMember("CategoryList_mc", &cat) || !cat.IsObject() ||
			!cat.GetMember("List_mc", &listObj) || !listObj.IsObject() ||
			!listObj.GetMember("entryList", &entryList) || !entryList.IsArray()) {
			return;
		}

		const std::uint32_t arraySize = entryList.GetArraySize();
		if (arraySize == 0)
			return;

		const char* menuName   = ("$FUCK_Title"_T);
		bool        isSafeMenu = page.HasMember("UpdateIndices");
		bool        removed    = false;

		for (std::uint32_t i = 0; i < arraySize; ++i) {
			RE::GFxValue element, textVal;
			if (entryList.GetElement(i, &element) && element.IsObject() && element.GetMember("text", &textVal) && textVal.IsString()) {
				if (std::string_view(textVal.GetString()) == menuName) {
					if (!isSafeMenu && manager->GetReplaceHelpMenu()) {
						element.SetMember("text", "$HELP");
						entryList.SetElement(i, element);
					} else {
						// Shift up to overwrite the injected entry
						for (std::uint32_t j = i; j < arraySize - 1; ++j) {
							RE::GFxValue temp;
							entryList.GetElement(j + 1, &temp);
							entryList.SetElement(j, temp);
						}

						entryList.SetArraySize(arraySize - 1);

						if (isSafeMenu) {
							page.Invoke("UpdateIndices", nullptr, nullptr, 0);
						}
					}
					removed = true;
					break;
				}
			}
		}

		if (removed) {
			listObj.Invoke("InvalidateData", nullptr, nullptr, 0);
		}
		s_fuckButtonInjected = false;
	}

	// Returns true if our injected entry is the currently highlighted System Menu item.
	static bool IsFUCKEntrySelected(RE::GFxMovieView* a_movieView)
	{
		if (!a_movieView) {
			return false;
		}

		RE::GFxValue page, cat, listObj, selIdx, entryList, selectedEntry, textVal;

		// Fetch the base object
		if (!a_movieView->GetVariable(&page, kSystemPagePath) || !page.IsObject()) {
			return false;
		}

		// Only process if the System menu is actually focused and in the MAIN_STATE (0).
		// This prevents wire-crossing when the user clicks inside sub-menus like the Quit confirmation.
		RE::GFxValue currentStateVal;
		if (page.GetMember("iCurrentState", &currentStateVal) && currentStateVal.IsNumber()) {
			if (currentStateVal.GetNumber() != 0.0) {
				return false;
			}
		}

		// Null check harder than my ex did
		if (!page.GetMember("CategoryList_mc", &cat)     || !cat.IsObject()       ||
			!cat.GetMember("List_mc", &listObj)          || !listObj.IsObject()   ||
			!listObj.GetMember("selectedIndex", &selIdx) || !selIdx.IsNumber()    ||
			!listObj.GetMember("entryList", &entryList)  || !entryList.IsArray())  {
			return false;
		}

		double rawIdx = selIdx.GetNumber();
		if (rawIdx < 0.0) {
			return false;
		}

		std::uint32_t index = static_cast<std::uint32_t>(rawIdx);
		if (index >= entryList.GetArraySize()) {
			return false;
		}

		if (!entryList.GetElement(index, &selectedEntry) || !selectedEntry.IsObject() ||
			!selectedEntry.GetMember("text", &textVal)   || !textVal.IsString())       {
			return false;
		}

		const char* textStr = textVal.GetString();
		if (!textStr) {
			return false;
		}

		const char* menuName = ("$FUCK_Title"_T);
		return std::string_view(textStr) == menuName;
	}

	// Closes the Journal and opens FUCK on the next frame.
	static void ActivateFUCK()
	{
		// It's go time
		ImGui::PlayAudio(ImGui::Audio::kOk);

		// Close the Journal menu natively so the game state clears
		if (auto queue = RE::UIMessageQueue::GetSingleton()) {
			queue->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
		}

		// Defer opening our menu to the next frame to prevent input collisions
		SKSE::GetTaskInterface()->AddTask([]() {
			FUCKMan::GetSingleton()->Open();
		});
	}

	struct JournalMenu_ProcessMessage
	{
		static RE::UI_MESSAGE_RESULTS thunk(RE::JournalMenu* a_this, RE::UIMessage& a_message)
		{
			// Reset tracking when the journal menu closes
			if (a_message.type == RE::UI_MESSAGE_TYPE::kHide) {
				s_fuckButtonInjected = false;
				return func(a_this, a_message);
			}

			// Activate FUCK when our injected entry is highlighted and accepted.
			// "Accept" (Enter/A/click) arrives as a user event, while the D-pad "Right" is swallowed by DirectionHandler and forwarded as a Scaleform key event.
			if (s_fuckButtonInjected && a_message.data && a_this->uiMovie) {
				bool activate = false;

				if (a_message.type == RE::UI_MESSAGE_TYPE::kUserEvent) {
					auto* data       = static_cast<RE::BSUIMessageData*>(a_message.data);
					auto* userEvents = RE::UserEvents::GetSingleton();
					activate         = userEvents && data->fixedStr == userEvents->accept;
				} else if (a_message.type == RE::UI_MESSAGE_TYPE::kScaleformEvent) {
					auto* event = static_cast<RE::BSUIScaleformData*>(a_message.data)->scaleformEvent;
					if (event && event->type == RE::GFxEvent::EventType::kKeyDown) {
						activate = static_cast<RE::GFxKeyEvent*>(event)->keyCode == RE::GFxKey::kRight;
					}
				}

				if (activate && IsFUCKEntrySelected(a_this->uiMovie.get())) {
					ActivateFUCK();
					return RE::UI_MESSAGE_RESULTS::kHandled;
				}
			}

			auto result = func(a_this, a_message);

			if (a_message.type == RE::UI_MESSAGE_TYPE::kUpdate && a_this->uiMovie) {
				auto* manager      = FUCKMan::GetSingleton();
				bool  shouldInject = manager->GetInjectSystemMenu() && MANAGER(Input)->IsInputGamepad();

				if (shouldInject && !s_fuckButtonInjected) {
					TryInjectFUCKButton(a_this->uiMovie.get());
				} else if (!shouldInject && s_fuckButtonInjected) {
					TryRemoveFUCKButton(a_this->uiMovie.get());
				}
			}

			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> journalVtbl(RE::VTABLE_JournalMenu[0]);
		JournalMenu_ProcessMessage::func = journalVtbl.write_vfunc(0x4, &JournalMenu_ProcessMessage::thunk);

		logger::info("Installed Journal Menu Hooks");
	}
}
