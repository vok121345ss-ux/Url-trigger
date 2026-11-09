#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

using namespace geode::prelude;

// URL trigger = обычный Text-объект (ID 914) с текстом:
//     url:https://example.com
// Когда игрок доходит до X-позиции объекта, открывается ссылка.
// Разрешены только http:// и https://

namespace {
	constexpr int TEXT_OBJECT_ID = 914;

	struct UrlEntry {
		float x = 0.f;
		std::string url;
		bool fired = false;
	};

	std::string trim(std::string s) {
		auto notSpace = [](unsigned char c) { return !std::isspace(c); };
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
		s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
		return s;
	}

	bool startsWithNoCase(std::string const& s, std::string const& prefix) {
		if (s.size() < prefix.size()) return false;
		for (size_t i = 0; i < prefix.size(); ++i) {
			if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) {
				return false;
			}
		}
		return true;
	}

	std::string parseUrl(std::string text) {
		text = trim(text);
		if (!startsWithNoCase(text, "url:")) return "";
		std::string url = trim(text.substr(4));
		if (!startsWithNoCase(url, "https://") && !startsWithNoCase(url, "http://")) return "";
		if (url.find_first_of(" \t\r\n") != std::string::npos) return "";
		return url;
	}
}

class $modify(UrlTriggerPlayLayer, PlayLayer) {
	struct Fields {
		std::vector<UrlEntry> entries;
	};

	void setupHasCompleted() {
		PlayLayer::setupHasCompleted();

		auto& entries = m_fields->entries;
		entries.clear();

		if (!m_objects) return;
		for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
			if (!obj || obj->m_objectID != TEXT_OBJECT_ID) continue;

			auto textObj = static_cast<TextGameObject*>(obj);
			auto url = parseUrl(std::string(textObj->m_text));
			if (url.empty()) continue;

			UrlEntry entry;
			entry.x = obj->getPositionX();
			entry.url = url;
			entries.push_back(entry);
		}

		std::sort(entries.begin(), entries.end(), [](UrlEntry const& a, UrlEntry const& b) {
			return a.x < b.x;
		});

		log::info("URL Trigger: found {} link(s) in this level", entries.size());
	}

	void resetLevel() {
		PlayLayer::resetLevel();

		float startX = m_player1 ? m_player1->getPositionX() : 0.f;
		for (auto& entry : m_fields->entries) {
			entry.fired = entry.x <= startX;
		}
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);

		auto& entries = m_fields->entries;
		if (entries.empty() || !m_player1 || m_player1->m_isDead) return;

		float playerX = m_player1->getPositionX();
		for (auto& entry : entries) {
			if (entry.fired) continue;
			if (playerX < entry.x) break;

			entry.fired = true;
			this->openLink(entry.url);
			break;
		}
	}

	void openLink(std::string url) {
		if (!Mod::get()->getSettingValue<bool>("ask-before-open")) {
			web::openLinkInBrowser(url);
			return;
		}

		this->pauseGame(false);
		createQuickPopup(
			"Open link?",
			fmt::format("This level wants to open a link in your browser:\n\n{}", url),
			"Cancel", "Open",
			[url](FLAlertLayer*, bool open) {
				if (open) web::openLinkInBrowser(url);
			}
		);
	}
};
