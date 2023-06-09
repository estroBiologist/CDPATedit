#include <iostream>
#include <functional>
#include <filesystem>
#include <cmath>


#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <SFML/Audio.hpp>

#include "imgui.h"
#include "imgui-SFML.h"
#include "imgui_stdlib.h"

#include "CDPAT.h"
#include "Actions.h"

#include "tinyfiledialogs.h"

#include "LICENSES.h"

#ifdef _WIN32
#include <Windows.h>
#endif

struct Stem {
	sf::SoundBuffer buffer{};
	sf::Sound player{};
	bool active = true;
	std::string name = "";
};

bool unsavedChangesWindow = false;
std::function<void()> unsavedChangesCallback;
cdpat::Pattern pattern{};
std::vector<std::string> directory{};
sf::SoundBuffer mus{};
sf::Sound mus_player{};
std::vector<Stem> stems;

bool resFolderFound = false;
bool helpPanelOpen = false;
std::string godot_path = "";

std::vector<cdpat::NoteRef> selectedNotes{};


bool isNoteSelected(const cdpat::NoteRef& noteRef) {
	return std::find_if(selectedNotes.begin(), selectedNotes.end(), [&](const cdpat::NoteRef& note) { return note.beat == noteRef.beat && note.lane == noteRef.lane; }) != selectedNotes.end();
}

void cancelSelection() {
	if (!selectedNotes.empty())
		pattern.applyAction<cdpat::DeselectAction>(selectedNotes);
}

void requestSaveAndCallback(std::function<void()>&& callback) {
	if (pattern.hasUnsavedChanges()) {
		unsavedChangesWindow = true;
		unsavedChangesCallback = callback;
	} else {
		callback();
	}
}

float secsToBeats(float secs, float bpm) {
	return secs * (bpm / 60.0f);
}

float beatsToSecs(float beats, float bpm) {
	return beats / (bpm / 60.0f);
}

float lerp(float a, float b, float t) {
	return a * (1.0f - t) + b * t;
}


void saveSettings() {
	std::ofstream ofs(".config");

	if (!ofs) {
		std::cout << "Error: failed to open .config file for writing\n";
		return;
	}

	ofs << cdpat::Pattern::res_path << "\n";
	ofs << godot_path << "\n";
	ofs << helpPanelOpen << "\n";
	ofs << cdpat::MAX_ACTION_HISTORY << "\n";
}

void loadSettings() {
	std::ifstream ifs(".config");

	if (!ifs) {
		std::cout << "Note: no .config file found. Using default settings";
		return;
	}

	std::getline(ifs, cdpat::Pattern::res_path);
	std::getline(ifs, godot_path);
	ifs >> helpPanelOpen;
	ifs >> cdpat::MAX_ACTION_HISTORY;
}

void refreshDirectory() {
	resFolderFound = false;

	if (cdpat::Pattern::res_path.empty()) {
		std::cout << "Error: no res:// folder specified.";
		return;
	}
	directory.clear();

	if (!std::filesystem::exists(cdpat::Pattern::res_path + "project.godot")) {
		std::cout << "Error: specified res:// folder is not a valid Godot project.";
		return;
	}

	if (!std::filesystem::is_directory(cdpat::Pattern::res_path + "patterns/")) {
		std::cout << "Error: specified res:// folder is not a valid CHORDIOID project.";
		return;
	}

	for (const auto& entry : std::filesystem::directory_iterator(cdpat::Pattern::res_path + "patterns/")) {
		if (entry.is_regular_file()) {
			std::string filename = entry.path().filename().string();
			size_t ext_start = filename.find_last_of(".");

			if (filename.substr(ext_start + 1) == "cdpat")
				directory.push_back(filename.substr(0, ext_start));
		}
	}
	resFolderFound = true;
}

void reloadAudio() {
	std::string resource_path = cdpat::Pattern::res_path + "music/rhythm/" + pattern.song_name + ".tres";
	std::cout << "Loading rhythm data from " << resource_path << "\n";
	//Load rhythm data
	std::ifstream ifs(resource_path);

	if (!ifs) {
		std::cerr << "Failed to open RhythmStream at location " << resource_path << "\n";
	}

	//Fucking christ we have to parse .tres files
	std::unordered_map<int, std::string> ext_resources;
	std::unordered_map <std::string, std::string> properties;
	std::unordered_map<std::string, std::vector<std::string>> arrays;

	while (ifs) {
		std::string token;
		ifs >> token;

		// Parse external resources
		if (token == "[gd_resource") {
			std::string junk;
			ifs >> junk; ifs >> junk; ifs >> junk;
			
			continue;

		} else if (token == "[ext_resource") {
			std::string path, type, id;
			ifs >> path; ifs >> type; ifs >> id;

			if (type == "type=\"AudioStream\"") {
				id = id.substr(3);
				id = id.substr(0, id.find("]"));
				int id_num = std::stoi(id);

				path = path.substr(std::string("path=\"res://").length());
				path.pop_back(); // Remove trailing quote

				ext_resources[id_num] = path;
			}
			continue;
		}

		if (token == "[resource]") {
			while (ifs) {
				std::string junk, name, val;
				
				ifs >> name;
				ifs >> junk;
				ifs >> val;

				std::vector<std::string> array_result;

				if (val == "ExtResource(") {
					ifs >> val; // value
					ifs >> junk; // ")"

					assert(junk == ")");

				} else if (val == "[") {
					std::string accum;
					
					do {
						ifs >> accum;
						val += accum;
						std::cout << accum << "\n";
					} 
					while (ifs && accum != "]");
				}

				properties[name] = val;
			}
		}

		pattern.bpm = std::stof(properties.at("bpm"));
		pattern.sig = std::stoi(properties.at("beats_per_bar"));

		break;
	}

	std::string song_path = cdpat::Pattern::res_path + ext_resources.at(std::stoi(properties.at("stream")));

	std::cout << "Loading audio stream from file " << song_path << "\n";
	if (!mus.loadFromFile(song_path)) {
		std::cout << "Failed loading stream, aborting music load\n";
		return;
	}
	mus_player.setLoop(true);
	mus_player.setBuffer(mus);
	mus_player.play();
	mus_player.pause();
	mus_player.setPlayingOffset(sf::Time{});


	// Hacky array parsing Lol
	std::string stems_txt = properties.at("stems");
	if (stems_txt != "[]") {
		while (!stems_txt.empty()) {
			stems_txt = stems_txt.substr(std::string{ "[ExtResource(" }.length());
			std::string index_txt = stems_txt.substr(0, stems_txt.find(")"));
			int index = std::stoi(index_txt);
			stems_txt = stems_txt.substr(index_txt.length() + 2); // num + )]

			Stem& stem = stems.emplace_back();
			stem.name = ext_resources.at(index);
			stem.buffer.loadFromFile(cdpat::Pattern::res_path + ext_resources.at(index));
			stem.player.setBuffer(stem.buffer);
			stem.player.play();
			stem.player.pause();
			stem.player.setPlayingOffset(sf::Time{});
		}
	}


}

void newPattern() {
	stems.clear();
	pattern = cdpat::Pattern{};
}


void loadPattern(std::string file) {
	pattern.load(file);
	stems.clear();
	reloadAudio();
}

void locateResFolder() {
	const char* filters[] = { "project.godot" };
	auto path_cstr = tinyfd_openFileDialog("Open project.godot file...", pattern.res_path.c_str(), 1, filters, "CHORDIOID Godot Project (project.godot)", 0);
	if (path_cstr) {
		std::string path{ path_cstr };

		std::replace(path.begin(), path.end(), '\\', '/');

		auto folder_ind = path.find_last_of("/");
		path = path.substr(0, folder_ind + 1);
		cdpat::Pattern::res_path = path;
		refreshDirectory();
	}
}

bool playing = false;

void startPlayback() {
	playing = true;
	mus_player.play();
	mus_player.setLoop(true);
	for (auto& stem : stems)
		if (stem.active) {
			stem.player.setLoop(false);
			stem.player.play();
			stem.player.setPlayingOffset(mus_player.getPlayingOffset());
		}
}

void stopPlayback() {
	playing = false;
	mus_player.pause();
	mus_player.setPlayingOffset(sf::Time{});
	for (auto& stem : stems) {
		stem.player.pause();
		stem.player.setPlayingOffset(sf::Time{});
	}
}

void pausePlayback() {
	playing = false;
	mus_player.pause();
	for (auto& stem : stems)
		stem.player.pause();
}

void togglePlayback() {
	playing = !playing;
	if (playing) 
		startPlayback();
	else 
		pausePlayback();
}

void setPlayingPosition(sf::Time position) {
	mus_player.setPlayingOffset(position);
	for (auto& stem : stems) 
		stem.player.setPlayingOffset(position);
	
}


float scroll_y = 0.0f;
float scroll_y_goal = -0.5f;


float start_offset = 256.0f;
float lane_offset = 128.0f;
float y_scale = 200.0f;

sf::Vector2f mapMouseToScreen(sf::Vector2i mousePos) {
	auto mousePosMapped = sf::Vector2f(mousePos);
	mousePosMapped.x -= start_offset;
	mousePosMapped.x /= lane_offset;
	mousePosMapped.y /= y_scale;
	mousePosMapped.y += scroll_y;
	return mousePosMapped;
}



int main() {
	using namespace cdpat;
	const std::array<std::string, 3> TOOLS {"Select", "Place", "Erase"};
	
	
	std::cout << "CDPATedit - By Ash Taylor\nSupported CDPAT version: " << cdpat::CDPAT_VERSION << "\n";
	
	sf::ContextSettings settings;
	settings.antialiasingLevel = 8;
	
	sf::RenderWindow window(sf::VideoMode(1280, 720), "CDPATedit", sf::Style::Default | sf::Style::Resize, settings);
	window.setVerticalSyncEnabled(true);
	ImGui::SFML::Init(window);
	
	
	//Windows specific
#ifdef _WIN32
	ShowWindow(window.getSystemHandle(), SW_MAXIMIZE);
#endif
	
	window.clear(sf::Color::Black);
	window.display();
	
	
	sf::Font ubuntuLight;
	ubuntuLight.loadFromFile("Resources/Ubuntu-Light.ttf");
	
	loadSettings();
	refreshDirectory();

	const auto highlightColor = sf::Color(255, 0, 42);
	
	sf::Vector2f mouseDragStart;
	bool mouseDragging = false;
	enum MouseDragMode {
		None,
		Select,
		ResizeHold,
	};
	MouseDragMode dragMode = None;
	std::function<void(sf::Vector2f)> mouseDragFinishCallback;
	
	float snap = 0.25f;	
	float noteRadius = 20.0f;

	sf::Clock framerateClock;
	unsigned framecount = 0;
	unsigned last_framecount = 0;
	sf::Clock deltaClock;
	while (window.isOpen()) {

		if (framerateClock.getElapsedTime().asSeconds() > 1.0f) {
			last_framecount = framecount;
			framecount = 0;
			framerateClock.restart();
		}


		auto mousePosMapped = mapMouseToScreen(sf::Mouse::getPosition(window));
		
		int lane = static_cast<int>(round(mousePosMapped.x));
		
		float beat = mousePosMapped.y;
			
		if (snap != 0.0f && !sf::Keyboard::isKeyPressed(sf::Keyboard::LAlt))
			beat = round(mousePosMapped.y * (1.0f / snap)) / (1.0f / snap);
		
		
		bool noteFound = false;
		const auto& events = pattern.getEvents();
							
		if (events.find(beat) == events.end())
			noteFound = false;
		else {
			for (const auto& event : events.at(beat)) {
				if (event.type == "note" && std::get<int>(event.args[0]) == lane) {
					noteFound = true;
				}
			}
		}

		//Find closest note, see if it's under a threshold
								
		auto low = events.lower_bound(mousePosMapped.y);
		const float threshold = .5f;
		float closest = -1.0f;
		float distA = INFINITY;
		float distB = INFINITY;
		cdpat::EventsData::const_iterator iterA;
		cdpat::EventsData::const_iterator iterB;

		if (low != events.end()) {
			//If `low` is the first event, prev is invalid
			auto prev = low == events.begin() ? events.end() : std::prev(low);

			// Distance to note below cursor
			distA = std::abs(low->first - mousePosMapped.y);

			distB = prev != events.end() ? std::abs(prev->first - mousePosMapped.y) : INFINITY;
			
			iterA = low;
			iterB = prev;
		} else {
			// Low == events.end(), so might be the last note in the sequence
			if (!events.empty()) {
				iterA = std::prev(events.end());
				distA = std::abs(iterA->first - mousePosMapped.y);
			}
		}

		bool isPrevClosest = distB < distA;

		if (distA < threshold || distB < threshold) {
			if (isPrevClosest)
				closest = iterB->first;
			else
				closest = iterA->first;
		}


		sf::Event w_event;
		while (window.pollEvent(w_event)) {
			ImGui::SFML::ProcessEvent(w_event);
			
			switch (w_event.type) {
				
				
				case sf::Event::Closed:
					requestSaveAndCallback([&]() { window.close(); });
					break;
				
				
				case sf::Event::Resized: {
					auto new_size = sf::Vector2f(w_event.size.width, w_event.size.height);
					window.setView(sf::View{new_size / 2.0f, new_size});
					break;
				}
				
				
				case sf::Event::MouseWheelScrolled:
					if (ImGui::GetIO().WantCaptureMouse) 
						break;

					if (sf::Keyboard::isKeyPressed(sf::Keyboard::LControl)) {
						float resize = snap;

						if (w_event.mouseWheelScroll.delta > 0) 
							resize = -resize;

						if (closest > 0.0f) 
							pattern.applyAction<ResizeHoldAction>(closest, lane, resize);


					} else {
						scroll_y_goal -= static_cast<float>(w_event.mouseWheelScroll.delta) * 0.75f;
					
						if (scroll_y_goal < -0.5f)
							scroll_y_goal = -0.5f;
					}
					break;
				
				
				case sf::Event::MouseButtonPressed: {
					if (ImGui::GetIO().WantCaptureMouse) 
						break;
					
					// Tool
					if (mousePosMapped.x < 0.f || mousePosMapped.x > 3.f)
						break;
						
					if (beat < 0.0f) 
						break;


					if (sf::Keyboard::isKeyPressed(sf::Keyboard::LControl)) {

						if (w_event.mouseButton.button == sf::Mouse::Left) {
							

							if (noteFound) {
								if (!sf::Keyboard::isKeyPressed(sf::Keyboard::LShift))
									cancelSelection();

								auto noteRef = cdpat::NoteRef{ beat, lane };
								pattern.applyAction<NoteSelectAction>(selectedNotes, std::vector{ noteRef }, !isNoteSelected(noteRef));
							} else {
								// Start dragging
								mouseDragStart = mousePosMapped;
								mouseDragging = true;
								dragMode = Select;

								mouseDragFinishCallback = [&](sf::Vector2f mouseDragFinish) {
									if (!sf::Keyboard::isKeyPressed(sf::Keyboard::LShift))
										cancelSelection();

									std::vector<cdpat::NoteRef> notesToSelect{};
									sf::Vector2f topLeft{ std::min(mouseDragStart.x, mouseDragFinish.x),std::min(mouseDragStart.y, mouseDragFinish.y) };
									sf::Vector2f bottomRight{ std::max(mouseDragStart.x, mouseDragFinish.x),std::max(mouseDragStart.y, mouseDragFinish.y) };
									
									auto low = events.lower_bound(topLeft.y);
									auto hi = events.upper_bound(bottomRight.y);

									if (low == events.end())
										return;

									for (auto it = low; it != hi; ++it) {
										auto events = it->second;

										for (const auto& event : events) {
											if (event.type == "note") {
												int lane = std::get<int>(event.args[0]);

												if (lane >= topLeft.x && lane <= bottomRight.x)
													notesToSelect.push_back({ it->first, lane });
											}
										}
									}
									if (!notesToSelect.empty())
										pattern.applyAction<NoteSelectAction>(selectedNotes, std::move(notesToSelect), true);
									
								};
							}
						}

					} else {
						// No Ctrl held, place or erase notes
						if (w_event.mouseButton.button == sf::Mouse::Left && !noteFound) {
							cancelSelection();
							pattern.applyAction<NoteAction>(std::vector{ cdpat::NoteRef{ beat, lane } }, true);
						}

						else if (w_event.mouseButton.button == sf::Mouse::Right && closest > 0.0f) {
							pattern.applyAction<NoteAction>(std::vector{ cdpat::NoteRef{ closest, lane } }, false);
						}
						
						break;
					}
						
				}
				break;

				case sf::Event::KeyPressed: {
					using namespace sf;
					
					if (ImGui::GetIO().WantCaptureKeyboard)
						break;
						
					if (Keyboard::isKeyPressed(Keyboard::LControl)) {
						
						switch (w_event.key.code) {
							case Keyboard::Z:
								pattern.undo();
								break;
							
							case Keyboard::Y:
								pattern.redo();
								break;
							
							case Keyboard::S:
								(void)pattern.saveToFile();
								refreshDirectory();
								break;
							
							default:
								break;
						}
						
					}
					
					switch (w_event.key.code) {
						case Keyboard::Space:
							togglePlayback();
							break;

						case Keyboard::Delete:
							if (!selectedNotes.empty())
								pattern.applyAction<NoteAction>(selectedNotes, false);
						default:
							break;
					}
				}
				break;
				
				default:
					break;
			}
		}
		
		
		
		//pattern.processLongAction();
		
		// Update
		
		if (sf::Mouse::isButtonPressed(sf::Mouse::Left) && mousePosMapped.x < 0.0f) {
			setPlayingPosition(sf::seconds(beatsToSecs(beat, pattern.bpm)));
			//TODO: Scroll at top and bottom of screen
		}

		if (!sf::Mouse::isButtonPressed(sf::Mouse::Left) && mouseDragging) {
			mouseDragFinishCallback(mousePosMapped);
			mouseDragging = false;
			dragMode = None;
		}
		
		
		
		std::string title = "CDPATedit - ";
		if (pattern.getPatternName().empty())
			title += "Untitled";
		else
			title += pattern.getPatternName();
		
		if (pattern.hasUnsavedChanges())
			title += "*";
		
		window.setTitle(title);
		





		// Playback position

		auto playback_col = sf::Color::White;

		if (playing)
			playback_col = sf::Color::Green;

		float playback_position = secsToBeats(mus_player.getPlayingOffset().asSeconds(), pattern.bpm);


		if (playing)
			scroll_y = scroll_y_goal = std::max(playback_position - 2.0f, 0.0f);
		else
			scroll_y = lerp(scroll_y, scroll_y_goal, 0.2f);


		static bool saveAsMenu = false;
		static std::string saveAsName = "";

		static bool licensesPanelOpen = false;

		// ImGui
	
		ImGui::SFML::Update(window, deltaClock.restart());
		
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("New")) {
					requestSaveAndCallback(newPattern);
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Save", "Ctrl+S", nullptr, resFolderFound)) {
					if (pattern.getPatternName().empty()) {
						saveAsMenu = true;
						saveAsName = "";
					}
					else {
						pattern.saveToFile();
						refreshDirectory();
					}
				}
				if (ImGui::MenuItem("Save as...", nullptr, nullptr, resFolderFound)) {
					saveAsMenu = true;
					saveAsName = "";
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Locate CHORDIOID folder...")) {
					locateResFolder();
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Quit")) {
					requestSaveAndCallback([&]() { window.close(); });
				}
				
				
				ImGui::EndMenu();
			}
			
			if (ImGui::BeginMenu("Edit")) {
				if (ImGui::MenuItem("Undo", "Ctrl+Z", false, pattern.canUndo())) pattern.undo();
				if (ImGui::MenuItem("Redo", "Ctrl+Y", false, pattern.canRedo())) pattern.redo();

				ImGui::Separator();

				ImGui::MenuItem("Cut", "Ctrl+X", false);
				ImGui::MenuItem("Copy", "Ctrl+C", false);
				ImGui::MenuItem("Paste", "Ctrl+V", false);

				ImGui::Separator();

				if (ImGui::MenuItem("Delete", "Del", false, !selectedNotes.empty()))
					pattern.applyAction<NoteAction>(selectedNotes, false);
					
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Run")) {
				if (ImGui::MenuItem("Run battle...")) {
				}
				if (ImGui::MenuItem("Run CHORDIOID...")) {
					std::string command = "\"" + godot_path + "\" --path \"" + Pattern::res_path + "\"";
					system(("\"start \"\" " + command + "\"").c_str());
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help")) {
				if (ImGui::MenuItem("Show Help Panel"))
					helpPanelOpen = true;

				if (ImGui::MenuItem("License Info"))
					licensesPanelOpen = true;

				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();

		}

		if (ImGui::Begin("Pattern")) {
			if (!resFolderFound) {
				ImGui::Text("Please locate the CHORDIOID project folder.");
			}
			else {
				ImGui::Text("Pattern list (click to open):");
				ImGui::PushItemWidth(-1);
				if (ImGui::BeginListBox("##Directory")) {

					for (const auto& file : directory) {
						if (ImGui::Selectable(file.c_str(), file == pattern.getPatternName()))
							requestSaveAndCallback([&]() { loadPattern(file); });
					}
					ImGui::EndListBox();
				}
				ImGui::PopItemWidth();

				bool patternUntitled = pattern.getPatternName().empty();

				if (patternUntitled)
					ImGui::Text("Opened pattern: (Untitled pattern)");
				else
					ImGui::Text("Opened pattern: %s", pattern.getPatternName().c_str());

				if (ImGui::Button("New"))
					requestSaveAndCallback(newPattern);

				ImGui::SameLine();
				if (ImGui::Button("Save")) {
					if (patternUntitled) {
						saveAsMenu = true;
						saveAsName = "";
					}
					else
						pattern.saveToFile();
				}

				ImGui::SameLine();
				if (ImGui::Button("Save as...")) {
					saveAsMenu = true;
					saveAsName = pattern.getPatternName();
				}

				ImGui::SameLine();
				if (ImGui::Button("Refresh"))
					refreshDirectory();



				ImGui::Text("");
				ImGui::Separator();



				ImGui::Text("Music");

				// Song data
				std::string song_name = pattern.song_name;
				int sig = pattern.sig;
				float bpm = pattern.bpm;

				ImGui::PushItemWidth(ImGui::GetWindowWidth() / 2.0);

				if (ImGui::InputText("Stream name", &song_name)) {
					pattern.applyAction<UpdateSongAction>(song_name);
				}

				ImGui::SameLine();
				if (ImGui::Button("Reload"))
					reloadAudio();

				ImGui::PopItemWidth();

				ImGui::Text("Stems");
				
				for (auto& stem: stems) {
					ImGui::Separator();
					ImGui::PushID(stem.name.c_str());
					ImGui::Text("%s", stem.name.c_str());

					if (ImGui::Checkbox("Active", &stem.active))
						pausePlayback();

					ImGui::PlotLines("##Waveform", 
						[](void* stem, int index) { 
							auto& buffer = static_cast<Stem*>(stem)->buffer;
							constexpr int scale = 50000;
							return static_cast<float>(buffer.getSamples()[index * scale]);
						}, 
						static_cast<void*>(&stem),
						stem.buffer.getSampleCount() / 50000);

					ImGui::PopID();
				}
			}
		}
		ImGui::End();



		if (ImGui::Begin("Configuration")) {

			ImGui::PushItemWidth(ImGui::GetWindowWidth() / 2.0);
			ImGui::InputText("Res:// folder", &Pattern::res_path, ImGuiInputTextFlags_ReadOnly);
			ImGui::SameLine();
			if (ImGui::Button("Locate"))
				locateResFolder();
			
			ImGui::InputText("Godot path", &godot_path);

#if INTPTR_MAX == INT32_MAX
			auto size_t_len = ImGuiDataType_U32;
#else
			auto size_t_len = ImGuiDataType_U64;
#endif
			const size_t step = 1U;

			ImGui::InputScalar("Max undo levels", size_t_len, &MAX_ACTION_HISTORY, &step);

			ImGui::PopItemWidth();

			float fps = 0.0f;
			if (last_framecount > 0) {
				fps = (float)last_framecount;
			}
			ImGui::Text("FPS: %f", fps);
		}
		ImGui::End();


		


		if (ImGui::Begin(("Events at position " + std::to_string(playback_position) + "###Events").c_str())) {

			if (!playing && events.count(playback_position)) {
				const auto& beat_events = events.at(playback_position);

				for (size_t i = 0; i < beat_events.size(); i++) {
					const auto& event = beat_events[i];

					std::string type = event.type;

					ImGui::Text("Event #%i", i);
					ImGui::PushID(i);
					
					ImGui::SameLine(ImGui::GetWindowWidth() - 30);
					
					if (ImGui::Button("X"))
						pattern.applyAction<EraseEventAction>(playback_position, i);

					if (ImGui::BeginCombo("Type", type.c_str())) {
						for (const auto& c_type: BEAT_EVENT_TYPES)
							if (ImGui::Selectable(c_type.c_str()))
								pattern.applyAction<EditEventTypeAction>(c_type, playback_position, i);
						ImGui::End();
					}
					
					//if (ImGui::InputText("Type", &type))
					//	pattern.applyAction<EditEventTypeAction>(type, playback_position, i);
					
					ImGui::PopID();

					ImGui::Separator();
				}
			}
			if (ImGui::Button("New"))
				pattern.applyAction<CreateEventAction>(playback_position);
		}
		ImGui::End();



		
		if (ImGui::Begin("View")) {
			ImGui::Text("Note Highway");
			ImGui::InputFloat("Y Scale", &y_scale, 1.0f, 5.0f);
			ImGui::InputFloat("X Offset", &start_offset, 0.5f, 1.0f);
			ImGui::InputFloat("Lane Width", &lane_offset, 8.0f, 16.f);
		}
		ImGui::End();
		
		
		if (ImGui::Begin("Tools")) {
			ImGui::Text("Playback");
		
			if (ImGui::Button(playing ? "Pause" : "Play")) 
				togglePlayback();
			
			ImGui::SameLine();
			
			if (ImGui::Button("Stop")) 
				stopPlayback();


			// Play offset information

			float playOffset = 0.0f;
			float totalLength = 0.0f;
			if (mus_player.getBuffer()) {
				playOffset = mus_player.getPlayingOffset().asSeconds();
				totalLength = mus_player.getBuffer()->getDuration().asSeconds();
			}
			float zero = 0.0f; // i love c++

			bool editedThisFrame = false;

			if (ImGui::SliderScalar("##PlayOffset", ImGuiDataType_Float, &playOffset, &zero, &totalLength, ""))
				mus_player.setPlayingOffset(sf::seconds(playOffset));

			ImGui::Text("%02d:%02d:%02d.%03d (measure %d, beat %d)",
				static_cast<int>(playOffset / 3600.f),
				static_cast<int>(playOffset / 60.f),
				static_cast<int>(std::fmod(playOffset, 60.f)),
				static_cast<int>(std::fmod(playOffset, 1.0f) * 1000), 
				static_cast<int>(playback_position) / pattern.sig,
				static_cast<int>(playback_position) % pattern.sig
			);
			
			ImGui::Separator();
			
			
			ImGui::Text("Tool");
			ImGui::Text("Snap");
			static size_t currentSnapIdx = 3;
			static std::vector<std::pair<std::string, float>> snapValues = {
				{"Beat", 1.0f},
				{"1/2 Beat", 0.5f},
				{"1/3 Beat", 1.0f / 3.0f},
				{"1/4 Beat", 0.25f},
				{"1/6 Beat", 1.0f / 6.0f},
				{"1/8 Beat", 0.125f},
				{"Disabled", 0.0f}
			};

			if (ImGui::BeginCombo("##Snap", snapValues[currentSnapIdx].first.c_str())) {
				for (int i = 0; i < snapValues.size(); i++) {
					if (ImGui::Selectable(snapValues[i].first.c_str())) {
						currentSnapIdx = i;
						snap = snapValues[i].second;
					}
				}
				ImGui::EndCombo();
			}
			
			ImGui::Separator();
			
			ImGui::Text("History");
			ImGui::PushItemWidth(-1);
			if (ImGui::BeginListBox("##History", ImVec2(-FLT_MIN, -FLT_MIN))) {
			
				const auto& actions = pattern.getActionsStack();
				
				for (long i = -1; i < static_cast<long>(actions.size()); i++) {
					
					std::string desc;
					
					if (i < 0)
						desc = "[End of history]";
					else
						desc = actions[i]->getDescription();
					
					
					bool disabled_color = false;
					
					// If this action has been undone, give it a disabled color (while still making it selectable)
					if (static_cast<long>(i) > pattern.getActionIndex()) {
						disabled_color = true;
						ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
					}
					
					if (ImGui::Selectable((desc + "##" + std::to_string(i)).c_str(), pattern.getActionIndex() == i))
						pattern.setActionIndex(i);
					
					// ...and be sure to pop it again, even if `i` changes in setActionIndex()
					if (disabled_color)
						ImGui::PopStyleColor();
				
				}
				
				ImGui::EndListBox();
			}
			
		}
		ImGui::End();
		

		if (helpPanelOpen) {
			if (ImGui::Begin("Help Panel", &helpPanelOpen)) {
				ImGui::Text("CDPATedit - by Ash Taylor");
				ImGui::Separator();
				ImGui::Text(R"RAWTEXT(Instructions:
Left-click on the highway to place notes.
Right-click to remove notes.

Use Ctrl+Mousewheel to add/remove holds.

Hold Alt to disable snapping.

To load music, enter the RhythmStream name into 
	Pattern > Song name.

(Example: res://music/rhythm/tutorial.tres
is loaded by entering "tutorial".)

)RAWTEXT");
			}
			ImGui::End();
		}

		

		if (saveAsMenu) {
			if (ImGui::Begin("Save as...")) {
				
				
				ImGui::Text("Please specify a pattern name.");
				ImGui::InputText("##Name", &saveAsName);

				ImGui::PushItemWidth(-1);
				if (ImGui::Button("Save")) {
					pattern.setPatternName(saveAsName);
					pattern.saveToFile();
					refreshDirectory();
					saveAsMenu = false;
				}
				ImGui::PopItemWidth();

				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
					saveAsMenu = false;

			}
			ImGui::End();
		}
		
		
		
		if (unsavedChangesWindow) {
			if (ImGui::Begin("Unsaved changes")) {
				
				ImGui::Text("This pattern has unsaved changes. Do you want to save them?");
				
				if (ImGui::Button("Save")) {
					pattern.saveToFile();
					refreshDirectory();
					unsavedChangesWindow = false;
					unsavedChangesCallback();
				}
				ImGui::SameLine();
				
				if (ImGui::Button("Discard")) {
					unsavedChangesWindow = false;
					unsavedChangesCallback();
				}
				
				ImGui::SameLine();
				
				if (ImGui::Button("Cancel"))
					unsavedChangesWindow = false;
			}
			ImGui::End();
		}
		
		
		if (licensesPanelOpen) {
			if (ImGui::Begin("License Info", &licensesPanelOpen))
				ImGui::Text("%s", LICENSE_INFO);
			ImGui::End();
		}

		

		
		
		
		
		//Rendering
		

		float min_visible_beat = -4.0f;
		float max_visible_beat = window.getSize().y / y_scale;


		window.clear();
		
		
		sf::CircleShape note;
		note.setRadius(noteRadius);
		note.setOrigin(note.getRadius(), note.getRadius());
		
		sf::RectangleShape hold;
		hold.setSize({20.f, 0.f});
		
		const std::array<unsigned, 4> lane_colors = {0xef233cff, 0x4545efff, 0xffff3cff, 0x3cff6dff};
		
		
		// Draw highway
		
		sf::VertexArray beatlines(sf::PrimitiveType::Lines);
		
		
		// Side lines
		
		beatlines.append(sf::Vertex({start_offset, 0.0f}, sf::Color::White));
		beatlines.append(sf::Vertex({start_offset, static_cast<float>(window.getSize().y)}, sf::Color::White));
		
		beatlines.append(sf::Vertex({start_offset + lane_offset * 3.f, 0.0f}, sf::Color::White));
		beatlines.append(sf::Vertex({start_offset + lane_offset * 3.f, static_cast<float>(window.getSize().y)}, sf::Color::White));
		
		beatlines.append(sf::Vertex({start_offset + lane_offset * 4.f, 0.0f}, sf::Color::White));
		beatlines.append(sf::Vertex({start_offset + lane_offset * 4.f, static_cast<float>(window.getSize().y)}, sf::Color::White));
		
		
		// Beat lines
		
		sf::Text beatNum;
		beatNum.setFont(ubuntuLight);
		beatNum.setCharacterSize(20);
		
		for (float f = std::floor(scroll_y + min_visible_beat); f < scroll_y + max_visible_beat; f += 1.0f) {
			float ypos = (f - scroll_y) * y_scale;
			
			auto color = sf::Color{255,255,255,127};
			
			beatlines.append(sf::Vertex({start_offset, ypos}, color));
			beatlines.append(sf::Vertex({start_offset + lane_offset * 3.f, ypos}, color));
			
			beatNum.setFillColor(color);
			
			beatNum.setPosition({start_offset + lane_offset * 4.f + 25.0f, ypos - beatNum.getGlobalBounds().height});
			beatNum.setString(std::to_string(static_cast<int>(f)));
			window.draw(beatNum);
			
			color.a = 50;
			
			beatlines.append(sf::Vertex({start_offset, ypos + 0.5f * y_scale}, color));
			beatlines.append(sf::Vertex({start_offset + lane_offset * 3.f, ypos + 0.5f * y_scale}, color));
		}
		
		
		beatlines.append(sf::Vertex({0.f, (playback_position - scroll_y) * y_scale}, playback_col));
		beatlines.append(sf::Vertex({start_offset + lane_offset * 3.f, (playback_position - scroll_y) * y_scale}, playback_col));
		
		window.draw(beatlines);
		
		
		// Draw waveform
		//sf::Sprite wavSprite;
		//wavSprite.setTexture(waveformTexture);
		//wavSprite.setColor(sf::Color::White);
		//wavSprite.setPosition(0, -scroll_y * y_scale);
		//window.draw(wavSprite);
		
		
		// Draw events
		if (!sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) && mousePosMapped.x >= 0.f && mousePosMapped.x <= 3.f) {
			note.setPosition({start_offset + lane_offset * lane, y_scale * (beat - scroll_y)});
			auto transColor = sf::Color{lane_colors[lane]};
			transColor.a = 127;
			note.setFillColor(transColor);
			window.draw(note);
		}
		
		const auto& p_events = pattern.getEvents();
		for (const auto& [beat, events] : p_events) {
			
			if (beat < scroll_y + min_visible_beat) continue;
			if (beat > scroll_y + max_visible_beat) break;
			
			
			for (const auto& event: events) {
				int lane = -1;
				float hold_length = 0.0f;

				// hit animation
				float anim_t = std::min(std::max((playback_position - beat) * 10.f, 0.0f), 1.0f);
					
				note.setRadius(lerp(noteRadius, 15.f, anim_t));
					
				note.setOrigin(note.getRadius(), note.getRadius());
					
				if (event.type == "note") {
					lane = std::get<int>(event.args[0]);
					

					if (isNoteSelected({ beat, lane })) {
						note.setOutlineColor(highlightColor);
						note.setOutlineThickness(3.0f);
					} else {
						note.setOutlineThickness(0.0f);
					}

					note.setFillColor(sf::Color(lane_colors[lane]));
					
					
					// Note hold
					
					if (event.args.size() > 1) {

						try {
							
							hold_length = std::get<float>(event.args[1]);
							
						} catch (const std::bad_variant_access& e) {
							
							(void)e;
							
							try {
								
								hold_length = static_cast<float>(std::get<int>(event.args[1]));
								
							} catch (const std::bad_variant_access& e2) {
								
								(void)e2;
								std::cerr << "Can't draw hold\n";
								
							}
							
						}
					}
				} else {

					lane = 4;
					note.setFillColor(sf::Color::White);

				}


				note.setPosition({start_offset + lane_offset * lane, y_scale * (beat - scroll_y)});

				hold.setPosition(note.getPosition());
				hold.move({hold.getSize().x / -2.f, 0.f});
						
				auto holdColor = note.getFillColor();
				holdColor.a = 127;
						
				hold.setFillColor(holdColor);
				hold.setSize({hold.getSize().x, hold_length * y_scale});
				window.draw(hold);
				window.draw(note);
			}
		}

		if (mouseDragging) {
			// Draw mouse drag effects
			switch (dragMode) {
			case Select: {
					sf::RectangleShape selectRect;
					sf::Vector2f startPosScr = { start_offset + lane_offset * mouseDragStart.x, y_scale * (mouseDragStart.y - scroll_y) };
					sf::Vector2f endPosScr = { start_offset + lane_offset * mousePosMapped.x, y_scale * (mousePosMapped.y - scroll_y) };
					selectRect.setPosition(startPosScr);
					selectRect.setSize(endPosScr - startPosScr);
					auto fillCol = highlightColor;
					fillCol.a = 50;
					selectRect.setFillColor(fillCol);
					selectRect.setOutlineColor(highlightColor);
					selectRect.setOutlineThickness(5.f);
					window.draw(selectRect);
				}
				break;

			case ResizeHold:

				break;

			default:

				break;
			}
		}

		
		ImGui::SFML::Render(window);
		window.display();
		framecount++;
	}

	saveSettings();
}

