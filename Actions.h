#pragma once
#include "CDPAT.h"

namespace cdpat {
	/*
	class TemplateAction : public Action {
	
	public:
		~TemplateAction() override = default;
		
		void apply(Pattern& pattern) override {
		
		}

		void undo(Pattern& pattern) override {

		}

		std::string getDescription() const override {

		}

		TemplateAction() {}

	};
	*/
	class PlaceNoteAction : public Action {
		int lane = 0;
		float hold_length = 0.0f;
		float beat = 0.0f;
	public:
		~PlaceNoteAction() override = default;

		void apply(Pattern& pattern) override {
			auto note = BeatEvent{"note", {lane}};

			if (hold_length > 0.0f)
				note.args.push_back(hold_length);

			auto& events_beat = getEventsData(pattern)[beat];
			
			assert(std::find_if(
				events_beat.begin(), 
				events_beat.end(), 
				[&](const BeatEvent& x) { return x.type == note.type && x.args[0] == note.args[0]; }) == events_beat.end()
			);

			getEventsData(pattern)[beat].push_back(note);
		}
		
		void undo(Pattern& pattern) override {
			getEventsData(pattern)[beat].pop_back();
		}
		
		std::string getDescription() const override {
			auto beat_string = std::to_string(beat);
			beat_string.erase ( beat_string.find_last_not_of('0') + 1, std::string::npos );
			
			if (beat_string.back() == '.')
				beat_string.resize(beat_string.size() - 1);

			return "Place note at (" + std::to_string(lane) + ", " + beat_string + ")";
		}

		PlaceNoteAction(float beat, int lane, float hold_length = 0.0f) : lane(lane), hold_length(hold_length), beat(beat) {}
	};

	class EraseNoteAction : public Action {
		int lane = 0;
		float beat = 0.0f;
		BeatEvent note_store;
		size_t index_store = 0;
	public:
		~EraseNoteAction() override = default;

		void apply(Pattern& pattern) override {
			auto& events = getEventsData(pattern)[beat];

			for (size_t i = 0; i < events.size(); i++) {
				if (events[i].type == "note" && std::get<int>(events[i].args[0]) == lane) {
					note_store = events[i];
					index_store = i;
					events.erase(events.begin() + i);
					break;
				}
			}
		}

		void undo(Pattern& pattern) override {
			auto& events = getEventsData(pattern)[beat];
			events.insert(events.begin() + index_store, note_store);
		}
		
		std::string getDescription() const override {
			auto beat_string = std::to_string(beat);
			beat_string.erase ( beat_string.find_last_not_of('0') + 1, std::string::npos );
			
			if (beat_string.back() == '.')
				beat_string.resize(beat_string.size() - 1);

			return "Erase note at (" + std::to_string(lane) + ", " + beat_string + ")";
		}
		
		EraseNoteAction(float beat, int lane) : lane(lane), beat(beat) {}

	};

	class ResizeHoldAction: public Action {
		int lane = 0;
		float beat = 0.0f;
		float delta = 0.25f;

		float originalLen = 0.0f;
	public:
		~ResizeHoldAction() override = default;

		void apply(Pattern& pattern) override {
			auto& events = getEventsData(pattern)[beat];

			for (auto& event : events) {
				if (event.type == "note" && std::get<int>(event.args[0]) == lane) {

					if (event.args.size() >= 2) {


						// Get hold length (either float or int)
						try {
							originalLen = std::get<float>(event.args[1]);
							
						} catch (const std::bad_variant_access& e) {
							(void)e;
							
							try {
								originalLen = std::get<int>(event.args[1]);

							} catch (const std::bad_variant_access& e2) {
								(void)e2;
								std::cerr << "Can't get hold length\n";
								
							}
						}

						// Resize
						event.args[1] = std::max(originalLen + delta, 0.0f);

					} else {
						originalLen = 0.0f;

						event.args.push_back(std::max(delta, 0.0f));
					}
				}
			}
		}

		void undo(Pattern& pattern) override {
			auto& events = getEventsData(pattern)[beat];

			for (auto& event : events)
				if (event.type == "note" && std::get<int>(event.args[0]) == lane)
					event.args[1] = originalLen;
		}

		std::string getDescription() const override {
			auto beat_string = std::to_string(beat);
			beat_string.erase ( beat_string.find_last_not_of('0') + 1, std::string::npos );
			
			if (beat_string.back() == '.')
				beat_string.resize(beat_string.size() - 1);

			return "Resize hold at (" + std::to_string(lane) + ", " + beat_string + ") by " + std::to_string(delta);
		}
		

		ResizeHoldAction(float beat, int lane, float delta) : lane(lane), beat(beat), delta(delta) {}
	};


	class EditEventTypeAction : public Action {
		std::string type;
		std::string old_type;
		float beat = 0.0f;
		size_t index = 0;
	public:
		~EditEventTypeAction() override = default;

		void apply(Pattern& pattern) override {
			auto& event = getEventsData(pattern).at(beat)[index];
			old_type = event.type;
			event.type = type;
		}

		void undo(Pattern& pattern) override {
			auto& event = getEventsData(pattern).at(beat)[index];
			event.type = old_type;
		}

		std::string getDescription() const override {
			return "Edit event type";
		}

		EditEventTypeAction(const std::string& type, float beat, size_t index) : type(type), beat(beat), index(index) {}

	};

	class EraseEventAction : public Action {
		BeatEvent store;
		float beat = 0.0f;
		size_t index = 0;
	public:
		~EraseEventAction() override = default;

		void apply(Pattern& pattern) override {
			auto& events = getEventsData(pattern).at(beat);
			store = events[index];
			events.erase(events.begin() + index);
		}

		void undo(Pattern& pattern) override {
			auto& events = getEventsData(pattern).at(beat);
			events.insert(events.begin() + index, store);
		}

		std::string getDescription() const override {
			return "Erase event at beat " + std::to_string(beat);
		}

		EraseEventAction(float beat, size_t index) : beat(beat), index(index) {}

	};

	class CreateEventAction : public Action {
		float beat = 0.0f;
		size_t index_store = 0;

		const cdpat::BeatEvent defaultEvent{ "hit", {} };
	public:
		~CreateEventAction() override = default;

		void apply(Pattern & pattern) override {
			getEventsData(pattern)[beat].push_back(defaultEvent);
		}

		void undo(Pattern & pattern) override {
			getEventsData(pattern).at(beat).pop_back();	
		}

		std::string getDescription() const override {
			return "Create event at beat " + std::to_string(beat);
		}

		CreateEventAction(float beat) : beat(beat) {}

	};

	class UpdateSongAction : public Action {
		std::string song_name;

		std::string sn_store;
	public:
		~UpdateSongAction() override = default;

		void apply(Pattern& pattern) override {
			sn_store = pattern.song_name;

			pattern.song_name = song_name;
		}

		void undo(Pattern& pattern) override {
			pattern.song_name = sn_store;
		}

		std::string getDescription() const override {
			return "Set song";
		}

		UpdateSongAction(std::string song_name) : song_name(song_name) {}

	};

	
}