#include <iostream>
#include <functional>
#include <filesystem>

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <SFML/Audio.hpp>

#include "imgui.h"
#include "imgui-SFML.h"
#include "imgui_stdlib.h"

#include "CDPAT.h"
#include "Actions.h"

#include "tinyfiledialogs.h"


#ifdef _WIN32
#include <Windows.h>
#endif



bool unsavedChangesWindow = false;
std::function<void()> unsavedChangesCallback;
cdpat::Pattern pattern{};
std::vector<std::string> directory{};
sf::SoundBuffer mus{};
sf::Sound mus_player{};

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


void refreshDirectory() {
	if (cdpat::Pattern::res_path.empty())
		return;
	directory.clear();
	for (const auto& entry : std::filesystem::directory_iterator(cdpat::Pattern::res_path + "patterns/")) {
		if (entry.is_regular_file()) {
			std::string filename = entry.path().filename().string();
			size_t ext_start = filename.find_last_of(".");

			if (filename.substr(ext_start + 1) == "cdpat")
				directory.push_back(filename.substr(0, ext_start));
		}
	}
}

void reloadAudio() {

	mus.loadFromFile(pattern.getSongPath());
	mus_player.setBuffer(mus);
	mus_player.play();
	mus_player.pause();
	mus_player.setPlayingOffset(sf::Time{});
}

void loadPattern(std::string file) {
	pattern.load(file); 
	reloadAudio();
}

int main() {
	using namespace cdpat;
	const std::array<std::string, 3> TOOLS {"Select", "Place", "Erase"};
	
	
	std::cout << "CDPATedit - By Ash Declan Taylor\nSupported CDPAT version: " << cdpat::CDPAT_VERSION << "\n";
	
	sf::ContextSettings settings;
	settings.antialiasingLevel = 8;
	
	
	sf::RenderWindow window(sf::VideoMode(1280, 720), "CDPATedit", sf::Style::Default | sf::Style::Resize, settings);
	window.setVerticalSyncEnabled(true);
	ImGui::SFML::Init(window);
	window.setFramerateLimit(60);
	
	
	//Windows specific
#ifdef _WIN32
	ShowWindow(window.getSystemHandle(), SW_MAXIMIZE);
#endif
	
	window.clear(sf::Color::Black);
	window.display();
	
	
	sf::Font ubuntuLight;
	ubuntuLight.loadFromFile("Resources/Ubuntu-Light.ttf");
	
	Pattern::res_path = "G:/Godot/CHORDIOID/";
	refreshDirectory();
	//pattern.load("test_rhythmbased");

	
	/*mus.loadFromFile(pattern.getSongPath());
	mus_player.setBuffer(mus);
	mus_player.play();
	mus_player.pause();
	mus_player.setPlayingOffset(sf::Time{});
	*/
	//sf::Image waveform{};
	//generateAudioWaveform(mus, waveform, sf::Vector2u(200, 10000));
	
	//sf::Texture waveformTexture{};
	//waveformTexture.loadFromImage(waveform);
	
	bool playing = false;
	
	float scroll_y = 0.0f;
	float scroll_y_goal = -0.5f;
	
	
	// TODO: Calculate
	float min_visible_beat = -16.0f;
	float max_visible_beat = 48.0f;
	
	
	float start_offset = 256.0f;
	float lane_offset = 128.0f;
	float y_scale = 200.0f;
	
	float snap = 0.25f;
	
	float noteRadius = 20.0f;

	
	enum class Tool {
		Place,
		Select,
		Erase,
	};
	
	Tool currentTool = Tool::Place;
	
	sf::Clock deltaClock;
	while (window.isOpen()) {
		auto mousePosMapped = sf::Vector2f(sf::Mouse::getPosition(window));
		mousePosMapped.x -= start_offset;
		mousePosMapped.x /= lane_offset;
		mousePosMapped.y /= y_scale;
		mousePosMapped.y += scroll_y;
		
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
		if (low != events.end()) {
			auto prev = std::prev(low);


			float distA = low != events.end() ? std::abs(low->first - mousePosMapped.y) : std::abs(prev->first - mousePosMapped.y);
			float distB = prev != events.end() ? std::abs(prev->first - mousePosMapped.y) : INFINITY;
			bool isPrevClosest = distB < distA || low == events.end();

			if (distA < threshold || distB < threshold) {
				if (isPrevClosest)
					closest = prev->first;
				else
					closest = low->first;
			}
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
					
					
					if (mousePosMapped.x < 0.f || mousePosMapped.x > 3.f)
						break;
						
					switch (currentTool) {
						case Tool::Place: {
							if (beat < 0.0f) 
								break;

							if (w_event.mouseButton.button == sf::Mouse::Left) {
								
								if (!noteFound)
									pattern.applyAction<PlaceNoteAction>(beat, lane, 0.0f);
								
							} else if (w_event.mouseButton.button == sf::Mouse::Right) {

								if (closest > 0.0f) 
									pattern.applyAction<EraseNoteAction>(closest, lane);

							} /*else { //Middle mouse button
								auto note_drag_action = MoveHoldLongAction();
								if (closest > 0.0f)
									pattern.initLongAction(&note_drag_action);
							}*/
							break;
						}
						default:
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
								break;
							
							default:
								break;
						}
						
					}
					
					switch (w_event.key.code) {
						case Keyboard::Space:
							playing = !playing;
							if (playing) mus_player.play();
							else mus_player.pause();
							break;

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
			mus_player.setPlayingOffset(sf::seconds(beatsToSecs(beat, pattern.bpm)));
			//TODO: Scroll at top and bottom of screen
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

		if (mus_player.getStatus() == sf::Sound::Playing)
			playback_col = sf::Color::Green;

		float playback_position = secsToBeats(mus_player.getPlayingOffset().asSeconds(), pattern.bpm);


		if (playing)
			scroll_y = scroll_y_goal = std::max(playback_position - 2.0f, 0.0f);
		else
			scroll_y = lerp(scroll_y, scroll_y_goal, 0.2f);


		// ImGui
	
		ImGui::SFML::Update(window, deltaClock.restart());
		
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("New")) {
					requestSaveAndCallback([&]() { pattern = Pattern{}; });
				}
				
				ImGui::Separator();
				
				if (ImGui::MenuItem("Save"))
					(void)pattern.saveToFile();

				if (ImGui::MenuItem("Locate CHORDIOID folder...")) {
					const char* filters[] = { "project.godot" };
					auto path_cstr = tinyfd_openFileDialog("Open project.godot file...", pattern.res_path.c_str(), 1, filters, "CHORDIOID Godot Project (project.godot)", 0);
					if (path_cstr) {
						std::string path{ path_cstr };
						auto folder_ind = path.find_last_of("/");
						if (folder_ind == -1)	
							folder_ind = path.find_last_of("\\");
						path = path.substr(0, folder_ind + 1);
						Pattern::res_path = path;
					}
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Quit")) {
					requestSaveAndCallback([&]() { window.close(); });
				}
				
				
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Edit")) {
				if (ImGui::MenuItem("Undo", nullptr, false, pattern.canUndo())) pattern.undo();
				if (ImGui::MenuItem("Redo", nullptr, false, pattern.canRedo())) pattern.redo();
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();

			//if (ImGui::BeginMenu("Run")) {
			//	if (ImGui::MenuItem("Launch CHORDIOID...", nullptr, false, !pattern.res_path.empty()))
			//		system(pattern.res_path+"")
			//}
		}

		static bool saveAsMenu = false;
		static std::string saveAsName = "";
		
		if (ImGui::Begin("Pattern")) {
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
				ImGui::Text("(Untitled pattern)");
			else
				ImGui::Text("%s", pattern.getPatternName().c_str());

			if (ImGui::Button("New"))
				requestSaveAndCallback([]() { pattern = Pattern{}; });

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




			ImGui::Separator();

			std::string song_name = pattern.song_name;
			int sig = pattern.sig;
			float bpm = pattern.bpm;

			if (ImGui::InputText("Song name", &song_name)) {
				pattern.applyAction<UpdateSongAction>(song_name);
			}

			ImGui::SameLine();
			if (ImGui::Button("Reload")) 
				reloadAudio();
			

			/*if (ImGui::InputFloat("Tempo", &bpm, 1.0f, 5.0f, "%.3f BPM") || ImGui::InputInt("Beats per bar", &sig, 1, 2)) {
				pattern.applyAction<UpdateSongAction>(song_name);
			}*/

			ImGui::Separator();
			ImGui::InputText("Res:// folder", &Pattern::res_path);
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
		
			if (ImGui::Button(playing ? "Pause" : "Play")) {
				playing = !playing;
				
				if (playing) mus_player.play();
				else mus_player.pause();
			}
			
			ImGui::SameLine();
			
			if (ImGui::Button("Stop")) {
				playing = false;
				// Pause at 0 instead of stopping so we can still move the play position
				mus_player.pause();
				mus_player.setPlayingOffset(sf::Time{});
			}
			
			
			ImGui::Separator();
			
			ImGui::Text("Tool");
			ImGui::InputFloat("##Snap", &snap);
			
			ImGui::SameLine(); if (ImGui::Button("x2")) snap *= 2.0f;
			ImGui::SameLine(); if (ImGui::Button("/2")) snap /= 2.0f;
			ImGui::SameLine(); ImGui::Text("Snap");
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
		
		

		if (saveAsMenu) {
			if (ImGui::Begin("Save as...")) {
				
				ImGui::InputText("##Name", &saveAsName);

				ImGui::PushItemWidth(-1);
				if (ImGui::Button("Save")) {
					pattern.setPatternName(saveAsName);
					pattern.saveToFile();
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
		
		
		

		
		
		
		
		//Rendering
		
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
		if (mousePosMapped.x >= 0.f && mousePosMapped.x <= 3.f) {
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
		
		ImGui::SFML::Render(window);
		window.display();
	}
}