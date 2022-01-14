#pragma once
#include <vector>
#include <map>
#include <variant>
#include <array>
#include <fstream>
#include <string>
#include <iostream>
#include <deque>

namespace cdpat {
	class Pattern;

	static inline const int CDPAT_VERSION = 150;

	static inline size_t MAX_ACTION_HISTORY = 100;
	
	const std::array<std::string, 4> BEAT_EVENT_TYPES { "note", "hit", "dialog", "target"};

	typedef std::variant<std::string, int, float> BeatEventArg;
	
	struct BeatEvent {
		std::string type;
		std::vector<BeatEventArg> args;
	};
	
	typedef std::map<float, std::vector<BeatEvent>> EventsData;

	class Action {
	public:
		virtual ~Action() = default;
		virtual void apply(Pattern& pattern) = 0;
		virtual void undo(Pattern& pattern) = 0;
		virtual std::string getDescription() const = 0;

	protected:
		static EventsData& getEventsData(Pattern& pattern);
	};


	class LongAction : public Action {
	public:
		virtual ~LongAction() = default;
		virtual void update(Pattern& pattern) = 0;
	};
	

	struct NoteRef {
		float beat;
		int lane;
	};
	


	class Pattern {
		EventsData events;

		std::deque<std::unique_ptr<Action>> actions_stack;
		long action_index = -1;

		std::string name = "";

		mutable bool unsaved_changes = false;



		std::string getPatternPath() const {
			return res_path + "/patterns/" + name + ".cdpat";
		}


		
		std::vector<BeatEvent>::iterator findNote(float beat, int lane) {
			for (size_t i = 0; i < events[beat].size(); i++) {
				if (events[beat][i].type == "note" && std::get<int>(events[beat][i].args[0]) == lane) {
					return events[beat].begin() + i;
				}
			}
			return events[beat].end();
		}


		bool loadFromFile(const std::string& filepath) {
			events.clear();

			std::cout << "Loading from " << filepath << "...\nres:// folder: " << res_path << "\n";

			std::ifstream ifs(filepath);

			if (!ifs)
				return false;

			std::string first_header;

			ifs >> first_header; // #cdpat

			if (first_header != "#cdpat") {
				std::cerr << "ERROR: Invalid CDPAT header.";
				return false;
			}

			ifs >> first_header; //Version

			if (std::stoi(first_header) > CDPAT_VERSION) {
				std::cerr << "ERROR: CDPAT version unsupported.";
				return false;
			}

			// Garbage
			std::getline(ifs, first_header);

			while (ifs) {
				std::string text;

				std::getline(ifs, text);

				if (text.empty())
					break;

				// Split by space

				std::vector<std::string> items;
				size_t pos;

				while ((pos = text.find(' ')) != std::string::npos) {
					items.push_back(text.substr(0, pos));
					text.erase(0, pos + 1);
				}
				items.push_back(text);



				// Content reading

				if (items[0][0] == '#') {
					// Header
					const auto& header = items[0];
					const auto& arg = items[1];

					if (header == "#bpm")
						bpm = std::stof(arg);
					else if (header == "#song")
						song_name = arg;
					else if (header == "#sig")
						sig = std::stoi(arg);

				}
				else {
					//Event reading

					float beat = std::stof(items[0]);
					BeatEvent event;

					// Find event type in list of known types

					if (std::find(BEAT_EVENT_TYPES.begin(), BEAT_EVENT_TYPES.end(), items[1]) != BEAT_EVENT_TYPES.end()) {
						event.type = items[1];

						if (items.size() > 2)
							for (unsigned i = 2; i < items.size(); i++) {

								if (items[i].empty()) continue;

								// Identify argument type

								bool isNumber = true;
								bool isFloat = false;

								for (const auto& c : items[i]) {
									if (c != '.' && (c < '0' || c > '9'))
										isNumber = false;

									if (c == '.' && !isNumber) {
										if (isFloat) { //More than one period, can't be a number
											isFloat = false;
											isNumber = false;
										}
										else
											isFloat = true;
									}
								}

								// Add argument to vector

								if (isNumber) {
									if (isFloat)
										event.args.emplace_back(std::stof(items[i]));
									else
										event.args.emplace_back(std::stoi(items[i]));
								}
								else {
									event.args.emplace_back(items[i]);
								}
							}

					}
					else {
						// Not a known event type

						std::cerr << "ERROR: Unrecognized event type '" << items[1] << "'\n";
						return false;
					}

					events[beat].push_back(event);
				}

			}

			return true;
		}
		
	public:
		bool hasUnsavedChanges() const {
			return unsaved_changes;
		}
		static inline std::string res_path = "";

		const std::string& getPatternName() const {
			return name;
		}

		void setPatternName(const std::string& name) {
			this->name = name;
		} 

		std::string getSongPath() const {
			return res_path + "music/mus_" + song_name + ".ogg";
		}
		
		const EventsData& getEvents() const {
			return events;
		}

		const std::deque<std::unique_ptr<Action>>& getActionsStack() const {
			return actions_stack;
		}

		long getActionIndex() const {
			return action_index;
		}

		bool canUndo() const {
			return action_index > -1;
		}

		bool canRedo() const {
			return action_index + 1 < actions_stack.size();
		}


		
		bool setActionIndex(long i) {
			if (i > action_index) {
				
				while (i > action_index) {
					if (canRedo()) 
						redo();
					else 
						return false;
				}
				
			} else if (i < action_index) {
				
				while (i < action_index) {
					if (canUndo())
						undo();
					else 
						return false;
				}
			}

			return true;
			
		}
		
		
		std::string song_name = "";
		float bpm = 120.f;
		int sig = 4;

		template<typename ActionType, typename... Args>
		void applyAction(Args... args) {
			std::unique_ptr<Action> action = std::make_unique<ActionType>(args...);
			unsaved_changes = true;
			
			if (action_index + 1 < actions_stack.size())
				actions_stack.resize(action_index + 1);

			while (actions_stack.size() > MAX_ACTION_HISTORY) 
				actions_stack.pop_front();
			

			action->apply(*this);
			std::cout << "Apply action: " << action->getDescription() << "\n";

			actions_stack.emplace_back(std::move(action));

			action_index++;
		}

		/*template<typename T>
		T* initLongAction(T&& action) {
			unsaved_changes = true;
			
			long_action_process = std::make_unique<T>(action);
			return dynamic_cast<T*>(long_action_process.get());
		}

			void processLongAction() {
				if (long_action_process)
					long_action_process->update(*this);
		}

		void finalizeLongAction(std::unique_ptr<LongAction>&& action) {
			if (action != long_action_process) throw std::runtime_error("Finalized action not currently being processed.");

			applyAction(std::move(action));
			long_action_process = nullptr;
		}*/
		

		bool undo() {
			if (actions_stack.empty()) return false;

			unsaved_changes = true;
			
			actions_stack[action_index]->undo(*this);
			std::cout << "Undo action: " << actions_stack[action_index]->getDescription() << "\n";

			action_index--;
			
			return true;
		}

		bool redo() {
			unsaved_changes = true;
			
			if (action_index + 1 == actions_stack.size()) 
				return false;
			
			actions_stack[++action_index]->apply(*this);
			std::cout << "Redo action: " << actions_stack[action_index]->getDescription() << "\n";
			
			return true;
		}
		
		bool load(const std::string& pattern_name) {
			if (res_path.empty()) 
				return false;

			if (loadFromFile(res_path + "patterns/" + pattern_name + ".cdpat")) {
				name = pattern_name;
				return true;
			}
			return false;
		}
		
		


		bool saveToFile() const {
			std::ofstream ofs(getPatternPath());
			
			if (!ofs) 
				return false;

			ofs << "#cdpat " << CDPAT_VERSION << "\n";
			ofs << "#song " << song_name << "\n";
			ofs << "#bpm " << bpm << "\n";
			ofs << "#sig " << sig << "\n";

			// Save events
			
			for (const auto& [beat, events] : events) {
				
				for (const auto& event: events) {
					
					ofs << beat << " " << event.type << " ";

					for (size_t i = 0; i < event.args.size(); i++) {
						const auto& arg = event.args[i];
						
						switch (arg.index()) {
							case 0: //string
								ofs << std::get<std::string>(arg);
								break;
							case 1: //int
								ofs << std::get<int>(arg);
								break;
							case 2: //float
								ofs << std::get<float>(arg);
								break;
						}

						if (i < event.args.size() - 1)
							ofs << " ";
					}

					ofs << "\n";
				}

			}

			unsaved_changes = false;
			
			return true;
		}

		
		friend class Action;
	};
}
