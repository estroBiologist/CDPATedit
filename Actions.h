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
	/*class PlaceNoteAction : public Action {
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

	};*/


	// Placing and erasing notes
	class NoteAction : public Action {
		std::vector<NoteRef> noteRefs;
		std::vector<BeatEvent> notes;
		std::vector<size_t> indices;
		bool isPlaceEvent = true;

		void placeNotes(Pattern& pattern) {
			bool append = indices.empty(); // for instance, when placing

			for (int i = 0; i < noteRefs.size(); i++) {
				auto& events = getEventsData(pattern)[noteRefs[i].beat];
				if (append)
					events.push_back(notes[i]);
				else
					events.insert(events.begin() + indices[i], notes[i]);

			}
			indices.clear();
			notes.clear();
		}
		
		void eraseNotes(Pattern& pattern) {
			notes.reserve(noteRefs.size());
			indices.reserve(noteRefs.size());

			for (const auto& noteRef : noteRefs) {
				auto& events = getEventsData(pattern)[noteRef.beat];

				for (size_t i = 0; i < events.size(); i++) {
					if (events[i].type == "note" && std::get<int>(events[i].args[0]) == noteRef.lane) {
						notes.push_back(events[i]);
						indices.push_back(i);
						events.erase(events.begin() + i);
						break;
					}
				}
			}
		}
	public:
		~NoteAction() override = default;

		void apply(Pattern& pattern) override {
			if (isPlaceEvent)
				placeNotes(pattern);
			else
				eraseNotes(pattern);
		}

		void undo(Pattern& pattern) override {
			if (isPlaceEvent)
				eraseNotes(pattern);
			else
				placeNotes(pattern);
		}

		std::string getDescription() const override {
			auto count = std::to_string(noteRefs.size());
			if (isPlaceEvent) {
				if (noteRefs.size() == 1) {

					auto beat_string = std::to_string(noteRefs[0].beat);
					beat_string.erase(beat_string.find_last_not_of('0') + 1, std::string::npos);

					if (beat_string.back() == '.')
						beat_string.resize(beat_string.size() - 1);

					return "Place note at (" + std::to_string(noteRefs[0].lane) + ", " + beat_string + ")";
				} else {
					return "Place " + count + " notes";
				}
			}
			else {
				if (noteRefs.size() == 1) {
					auto beat_string = std::to_string(noteRefs[0].beat);
					beat_string.erase(beat_string.find_last_not_of('0') + 1, std::string::npos);
				
					if (beat_string.back() == '.')
						beat_string.resize(beat_string.size() - 1);

					return "Erase note at (" + std::to_string(noteRefs[0].lane) + ", " + beat_string + ")";
				}
				else {
					return "Erase " + count + " notes";
				}
			}
		}

		NoteAction(std::vector<NoteRef> noteRefs, bool isPlaceEvent) : noteRefs(noteRefs), isPlaceEvent(isPlaceEvent) {
			if (isPlaceEvent) {
				// Create notes in memory to place
				notes.reserve(noteRefs.size());
				for (const auto& noteRef : noteRefs) {
					notes.push_back(BeatEvent{ "note", {noteRef.lane} });
				}
			}
		}

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


	class NoteSelectAction : public Action {
		std::vector<cdpat::NoteRef>& selectedNotesRef;
		std::vector<cdpat::NoteRef> notesToModify;
		bool addToSelection;

		void addNotes() {
			selectedNotesRef.reserve(selectedNotesRef.size() + notesToModify.size());

			// this doesn't work if selectedNotesRef == notesToModify (ie when deselecting)
			selectedNotesRef.insert(selectedNotesRef.end(), notesToModify.begin(), notesToModify.end());
			
		}

		void removeNotes() {
			selectedNotesRef.resize(selectedNotesRef.size() - notesToModify.size());
		}

	public:
		~NoteSelectAction() override = default;

		void apply(Pattern & pattern) override {
			if (addToSelection)
				addNotes();
			else
				removeNotes();
		}

		void undo(Pattern & pattern) override {
			if (addToSelection)
				removeNotes();
			else
				addNotes();
		}

		std::string getDescription() const override {
			auto num = std::to_string(notesToModify.size());
			if (addToSelection)
				return notesToModify.size() > 1 ? "Select " + num + " notes" : "Select 1 note";
			else
				return notesToModify.size() > 1 ? "Deselect " + num + " notes" : "Deselect 1 note";
		}

		NoteSelectAction(std::vector<cdpat::NoteRef>& selectedNotesRef, std::vector<cdpat::NoteRef>&& notesToModify, bool addToSelection) 
			: selectedNotesRef(selectedNotesRef), 
			notesToModify(std::move(notesToModify)), 
			addToSelection(addToSelection) {}
	};

	class DeselectAction : public Action {
		std::vector<cdpat::NoteRef>& selectedNotesRef;
		std::vector<cdpat::NoteRef> notesToModify;

	public:
		~DeselectAction() override = default;

		void apply(Pattern& pattern) override {
			notesToModify = std::move(selectedNotesRef);
		}

		void undo(Pattern& pattern) override {
			selectedNotesRef = std::move(notesToModify);
		}

		std::string getDescription() const override {
			return "Cancel selection";
		}

		DeselectAction(std::vector<cdpat::NoteRef>& selectedNotesRef)
			: selectedNotesRef(selectedNotesRef) {}
	};
	
}