#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

// ---------------------- Load Functions ----------------

GLuint heart_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > heart_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("HeartbeatSurvival.pnct"));
	heart_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > main_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("HeartbeatSurvival.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = heart_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = heart_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
	});
});

Load< Sound::Sample > easy_music_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("TaikoLoop.opus"));
});
Load< Sound::Sample > hard_music_sample(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("Taiko2Loop.opus"));
});
Load< Sound::Sample > menu_music_sample(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("TaikoBeach.opus"));
});
Load< Sound::Sample > death_music_sample(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("TaikoDeath.opus"));
});
Load< Sound::Sample > negative_sfx_sample(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("NegativeSFX.opus"));
});

// --------------------------------------

PlayMode::PlayMode() : scene(*main_scene) {
	// Get pointers to hearts
	for (auto &transform : scene.transforms) {
		if (transform.name == "good_heart") good_heart = &transform;
		else if (transform.name == "mid_heart") mid_heart = &transform;
		else if (transform.name == "bad_heart") bad_heart = &transform;
	}
	if (good_heart == nullptr) throw std::runtime_error("good_heart not found.");
	if (mid_heart == nullptr) throw std::runtime_error("mid_heart not found.");
	if (bad_heart == nullptr) throw std::runtime_error("bad_heart not found.");

	// Initialize heart variables and positions
	heart_base_pos = good_heart->position;
	heart_base_rotation = good_heart->rotation;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	
	setup_menu();
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_SPACE) {
			space.downs += 1;
			space.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_g) {
			g.downs += 1;
			g.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_e) {
			e.downs += 1;
			e.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_d) {
			d.downs += 1;
			d.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_s) {
			s.downs += 1;
			s.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_1) {
			one.downs += 1;
			one.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_2) {
			two.downs += 1;
			two.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_ESCAPE) {
			esc.downs += 1;
			esc.pressed = true;
			return true;
		}
	} 
	else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_SPACE) {
			space.pressed = false;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_g) {
			g.pressed = false;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_e) {
			e.pressed = false;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_d) {
			d.pressed = false;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_s) {
			s.pressed = false;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_1) {
			one.pressed = false;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_2) {
			two.pressed = false;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_ESCAPE) {
			esc.pressed = false;
			return true;
		}
	} 

	return false;
}

// ------------ Update Functions -----------------

void PlayMode::update(float elapsed) {

	switch (game_state) {
		case game:
			game_update(elapsed);
			break;
		case dead:
			death_update(elapsed);
			break;
		default:
		case menu:
			menu_update(elapsed);
			break;
	}

	// Reset button press counters:
	space.downs = 0;
	g.downs = 0;
	e.downs = 0;
	d.downs = 0;
	s.downs = 0;
	one.downs = 0;
	two.downs = 0;
	esc.downs = 0;
}

void PlayMode::game_update(float elapsed) {

	// Check for quit key
	if (esc.downs == 1) {
		setup_menu();
		return;
	}

	// Update beat detection timer
	timer -= elapsed;

	// Update grid flashing timer
	if (grid_state != neutral) {
		grid_timer -= elapsed;
		if (grid_timer < 0.0f) {
			grid_state = neutral;
		}
	}

	// Make grid grey on beat within tolerance window to prompt input, overridden by correct or incorrect presses
	if (grid_state == neutral && timer >= -timing_tolerance && timer <= timing_tolerance + elapsed) {
		grid_state = prompt;
	}

	// Player missed the last beat so reset the timer
	if (timer < -timing_tolerance) {

		timer = bpm + timer;

		if (!sleeping) {
			missed_beats++;
			fatigue.update_cur(-1);

			// Flash grid red
			grid_timer = grid_flash_duration;
			grid_state = negative;
		}
		else {
			hits++;
			fatigue.update_cur(1);
		}

		hunger.update_cur(-1);
		thirst.update_cur(-1);
	}

	// Check for on-beat input
	if (g.downs == 1 || e.downs == 1 || d.downs == 1 || s.downs == 1) {
		// input on time
		if (timer >= -timing_tolerance && timer <= timing_tolerance + elapsed) {
			
			// Any key press wakes player up
			if (sleeping) {
				sleeping = false;
				music_loop->set_volume(1.0f);
			}

			timer = bpm + timer; // reset timer and account for error within tolerance window
			hits++;
			// Flash grid green
			grid_timer = grid_flash_duration;
			grid_state = positive;

			// fatigue decreases unless player sleeps
			fatigue.update_cur(-1);

			// Handle key specific logic
			// Gather
			if (g.downs == 1) {
				if (std::rand() % 2 == 0) {
					food.update_cur(1);
				}
				else {
					water.update_cur(1);
				}
			}
			// Eat
			if (e.downs == 1) {
				if (food.cur > 0) {
					food.update_cur(-1);
					hunger.update_cur(1);
				}
				else {
					Sound::play(*negative_sfx_sample, 1.5f);
				}
			}
			// Drink
			if (d.downs == 1) {
				if (water.cur > 0) {
					water.update_cur(-1);
					thirst.update_cur(1);
				}
				else {
					Sound::play(*negative_sfx_sample, 1.5f);
				}
			}
			// Sleep
			if (s.downs == 1) {
				sleeping = true;
				music_loop->set_volume(0.3f);
			}
		}
		// input off-beat
		else {
			misses++;
			// Flash grid red
			grid_timer = grid_flash_duration;
			grid_state = negative;
		}
	}

	// Set grid color
	switch (grid_state) {
		case negative:
			grid_color = glm::u8vec4(0xff, 0x00, 0x00, 0xff);
			break;
		case positive:
			grid_color = glm::u8vec4(0x00, 0xff, 0x00, 0xff);
			break;
		case prompt:
			grid_color = glm::u8vec4(0xdd, 0xff, 0xdd, 0xff);
			break;
		default:
			grid_color = glm::u8vec4(0xff, 0xff, 0xff, 0xff);
	}

	// Update scrolling text position
	message_offset += message_speed * elapsed;

	// Check for death and set proper heart
	switch (get_overall_health()) {
		case zero:
			to_death_screen();
			return;
		case poor:
			swap_heart(bad_heart);
			break;
		case okay:
			swap_heart(mid_heart);
			break;
		case good:
			swap_heart(good_heart);
			break;
	}

	// Rotate Heart
	cur_heart->rotation *= glm::angleAxis(glm::radians(elapsed * 18.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	// Beat heart in time
	float lerp_t = ((bpm - timer) / bpm);
	float sin = std::sin(lerp_t * float(M_PI));
	float scalar = (std::abs(sin) * 0.2f) + 0.8f;
	cur_heart->scale = glm::vec3(scalar);
}

void PlayMode::menu_update(float elapsed) {

	if (one.downs == 1) {
		setup_new_round(false);
	}
	else if (two.downs == 1) {
		setup_new_round(true);
	}
}

void PlayMode::death_update(float elapsed) {

	if (esc.downs == 1) {
		setup_menu();
		return;
	}
}

// -------------- Draw Functions ---------------

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	switch (game_state) {
		case game:
			game_draw_ui(drawable_size);
			break;
		case dead:
			death_draw_ui(drawable_size);
			break;
		default:
		case menu:
			menu_draw_ui(drawable_size);
			break;
	}

	GL_ERRORS();
}

void PlayMode::game_draw_ui(glm::uvec2 const& drawable_size) {

	glDisable(GL_DEPTH_TEST);
	float aspect = float(drawable_size.x) / float(drawable_size.y);
		
	// Draw grid
	DrawLines grid(glm::mat4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	));
	// Grid
	grid.draw(glm::vec3(-2, 0.07f, 0), glm::vec3(2, 0.07f, 0), grid_color);
	grid.draw(glm::vec3(-0.33f, 2, 0), glm::vec3(-0.33f, -0.85f, 0), grid_color);
	grid.draw(glm::vec3(0.33f, 2, 0), glm::vec3(0.33f, -0.85f, 0), grid_color);
	grid.draw(glm::vec3(-2, -0.85f, 0), glm::vec3(2, -0.85f, 0), grid_color);
	// Frame
	grid.draw(glm::vec3(-2, 0.998f, 0), glm::vec3(2, 0.998f, 0), grid_color);
	grid.draw(glm::vec3(-2, -0.998f, 0), glm::vec3(2, -0.998f, 0), grid_color);
	grid.draw(glm::vec3(-0.998f, 2, 0), glm::vec3(-0.998f, -2, 0), grid_color);
	grid.draw(glm::vec3(0.998f, 2, 0), glm::vec3(0.998f, -2, 0), grid_color);


	// Draw Text
	DrawLines lines(glm::mat4(
		1.0f / aspect, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	));

	// Specifies text heights
	constexpr float H1 = 0.2f;
	constexpr float H2 = 0.08f;

	// Scrolling Text
	lines.draw_text(messages[cur_message_ind],
		glm::vec3(aspect - message_offset * H2, -1.0 + 0.45f * H2, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0x00, 0x00, 0x00, 0x00));
	float ofs = 2.0f / drawable_size.y;
	lines.draw_text(messages[cur_message_ind],
		glm::vec3(aspect - message_offset * H2 + ofs, -1.0 + 0.45f * H2 + ofs, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0x00),
		message_anchor_out);

	// Check to see if the rightmost edge of the text has scrolled off the left side of the screen
	if (message_anchor_out->x < -aspect) {
		// Set next message, loop back to beginning if neccesary
		cur_message_ind = (cur_message_ind + 1) % messages.size();
		// Set text so that lest edge of new leftmost character is just off the right side of the screen
		message_offset = 0;
	}

	
	lines.draw_text("Days Survied : " + std::to_string((missed_beats + hits) / 4), // total # of measures played
		glm::vec3(0.35f * aspect, (-H2 * aspect) + 0.1f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0xff));
	lines.draw_text("Deaths: " + std::to_string(deaths),
		glm::vec3(0.35f * aspect, (-2 * H2 * aspect) + 0.1f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0xff));

	// Timing stats text
	lines.draw_text("Hits : " + std::to_string(hits),
		glm::vec3(0.35f * aspect, (-3 * H2 * aspect) + 0.1f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xdd, 0xdd, 0xdd, 0xff));
	lines.draw_text("Missed beats : " + std::to_string(missed_beats),
		glm::vec3(0.35f * aspect, (-4 * H2 * aspect) + 0.1f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xdd, 0xdd, 0xdd, 0xff));
	lines.draw_text("Misses : " + std::to_string(misses),
		glm::vec3(0.35f * aspect, (-5 * H2 * aspect) + 0.1f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xdd, 0xdd, 0xdd, 0xff));

	lines.draw_text("[ Esc ] ape to menu",
		glm::vec3(0.35f * aspect, (-6.2f * H2 * aspect) + 0.1f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0xff));

	// Player stats text
	lines.draw_text("Hunger",
		glm::vec3((-3.0f / 4.0f) * aspect - 0.07f, (H1 * aspect) + 0.25f, 0.0),
		glm::vec3(H1, 0.0f, 0.0f), glm::vec3(0.0f, H1, 0.0f),
		get_stat_text_color(hunger));
	lines.draw_text(std::to_string(hunger.cur) + "/" + std::to_string(hunger.max),
		glm::vec3((-3.0f / 4.0f) * aspect + 0.08f, (H2 * aspect) + 0.23f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		get_stat_text_color(hunger));
	lines.draw_text("[ E ] at",
		glm::vec3((-3.0f / 4.0f) * aspect + 0.06f, (H2 * aspect) + 0.05f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0xff));

	lines.draw_text("Food",
		glm::vec3((-3.0f / 4.0f) * aspect - 0.02f, (H1 * -aspect) + 0.05f, 0.0),
		glm::vec3(H1, 0.0f, 0.0f), glm::vec3(0.0f, H1, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0xff));
	lines.draw_text(std::to_string(food.cur),
		glm::vec3((-3.0f / 4.0f) * aspect + 0.13f, (H2 * -aspect) - 0.32f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0xff));
	lines.draw_text("[ G ] ather",
		glm::vec3((-3.0f / 4.0f) * aspect + 0.0f, (H2 * -aspect) - 0.47f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0xff));

	lines.draw_text("Thirst",
		glm::vec3(-0.22f, (H1 * aspect) + 0.25f, 0.0),
		glm::vec3(H1, 0.0f, 0.0f), glm::vec3(0.0f, H1, 0.0f),
		get_stat_text_color(thirst));
	lines.draw_text(std::to_string(thirst.cur) + "/" + std::to_string(thirst.max),
		glm::vec3(-0.09f, (H2 * aspect) + 0.23f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		get_stat_text_color(thirst));
	lines.draw_text("[ D ] rink",
		glm::vec3(-0.13f, (H2 * aspect) + 0.05f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0xff));

	lines.draw_text("Water",
		glm::vec3(-0.16, (H1 * -aspect) + 0.05f, 0.0),
		glm::vec3(H1, 0.0f, 0.0f), glm::vec3(0.0f, H1, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0xff));
	lines.draw_text(std::to_string(water.cur),
		glm::vec3(0.0f, (H2 * -aspect) - 0.32f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0xff));
	lines.draw_text("[ G ] ather",
		glm::vec3(-0.15f, (H2 * -aspect) - 0.47f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0xff));

	lines.draw_text("Fatigue",
		glm::vec3((1.0f / 2.0f) * aspect + 0.07f, (H1 * aspect) + 0.25f, 0.0),
		glm::vec3(H1, 0.0f, 0.0f), glm::vec3(0.0f, H1, 0.0f),
		get_stat_text_color(fatigue));
	lines.draw_text(std::to_string(fatigue.cur) + "/" + std::to_string(fatigue.max),
		glm::vec3((1.0f / 2.0f) * aspect + 0.20f, (H2 * aspect) + 0.23f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		get_stat_text_color(fatigue));
	lines.draw_text("[ S ] leep",
		glm::vec3((1.0f / 2.0f) * aspect + 0.16f, (H2 * aspect) + 0.05f, 0.0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0xff));
}

void PlayMode::menu_draw_ui(glm::uvec2 const& drawable_size) {

	glDisable(GL_DEPTH_TEST);
	float aspect = float(drawable_size.x) / float(drawable_size.y);

	DrawLines lines(glm::mat4(
		1.0f / aspect, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	));

	// Specifies text heights
	constexpr float H1 = 0.4f;
	constexpr float H2 = 0.1f;

	// Title
	lines.draw_text("HEARTBEAT SURVIVAL",
		glm::vec3(-aspect + 0.05f, -0.95f, 0),
		glm::vec3(H1, 0.0f, 0.0f), glm::vec3(0.0f, H1, 0.0f),
		glm::u8vec4(0xff, 0xff, 0xff, 0xff));

	// Start Prompt
	lines.draw_text("[ 1 ] for easy  /  [ 2 ] for hard",
		glm::vec3(-0.6f, 0, 0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xcc, 0xcc, 0xcc, 0xff));
}

void PlayMode::death_draw_ui(glm::uvec2 const& drawable_size) {

	glDisable(GL_DEPTH_TEST);
	float aspect = float(drawable_size.x) / float(drawable_size.y);

	DrawLines lines(glm::mat4(
		1.0f / aspect, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	));

	constexpr float H1 = 0.4f;
	constexpr float H2 = 0.1f;

	lines.draw_text("YOU DIED",
		glm::vec3(-0.6f, 0, 0),
		glm::vec3(H1, 0.0f, 0.0f), glm::vec3(0.0f, H1, 0.0f),
		glm::u8vec4(0xFF, 0x00, 0x00, 0xff));

	// Prompt
	lines.draw_text("[ Esc ] ape",
		glm::vec3(-0.2f, -0.2f, 0),
		glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
		glm::u8vec4(0xcc, 0xcc, 0xcc, 0xff));
}

// --------------- Helper Functions -------------------------

glm::u8vec4 PlayMode::get_stat_text_color(PlayerStat stat) {

	switch (stat.status) {
		case zero:
			return glm::u8vec4(0x00, 0x00, 0x00, 0xff);
			break;
		case poor:
			return glm::u8vec4(0xff, 0x99, 0x99, 0xff);
			break;
		case good:
			return glm::u8vec4(0x99, 0xff, 0x99, 0xff);
			break;
		default:
		case okay:
			return glm::u8vec4(0xff, 0xff, 0xff, 0xff);
			break;
	}
}

PlayMode::StatStatus PlayMode::get_overall_health() {

	return (StatStatus)std::min({ (int)hunger.status, (int)thirst.status, (int)fatigue.status });
}

void PlayMode::swap_heart(Scene::Transform* new_heart) {

	if (cur_heart != new_heart) {
		new_heart->position = heart_base_pos;
		new_heart->rotation = cur_heart->rotation;
		new_heart->scale = cur_heart->scale;

		cur_heart->position = heart_hidden_pos;
		cur_heart->scale = glm::vec3(1, 1, 1);

		cur_heart = new_heart;
	}
}

void PlayMode::reset_heart() {

	mid_heart->position = heart_hidden_pos;
	bad_heart->position = heart_hidden_pos;

	good_heart->position = heart_base_pos;
	good_heart->scale = glm::vec3(1, 1, 1);
	good_heart->rotation = heart_base_rotation;
	
	cur_heart = good_heart;
}

void PlayMode::initialize_player_stats(bool is_hard_mode) {

	if (is_hard_mode) {
		hunger.max = 10;
		hunger.cur = 8;
		thirst.max = 10;
		thirst.cur = 8;
		food.max = 10;
		food.cur = 0;
		water.max = 10;
		water.cur = 0;
		fatigue.max = 15;
		fatigue.cur = 15;
		hunger.update_status();
		thirst.update_status();
		fatigue.update_status();
	}
	else {
		hunger.max = 15;
		hunger.cur = 15;
		thirst.max = 15;
		thirst.cur = 15;
		food.max = 100;
		food.cur = 2;
		water.max = 100;
		water.cur = 2;
		fatigue.max = 15;
		fatigue.cur = 15;
		hunger.update_status();
		thirst.update_status();
		fatigue.update_status();
	}

	hits = 0;
	missed_beats = 0;
	misses = 0;
}

void PlayMode::setup_new_round(bool is_hard_mode) {
	
	if (music_loop) {
		music_loop->stop();
	}

	// Set up music and timing variables
	if (is_hard_mode) {
		bpm = 60.0f / 90.0f; // 60 / bpm of Taiko2
		timer = bpm;
		timing_tolerance = bpm / 8.0f; // Can miss by up to an eighth of a beat and still count as a hit
		music_loop = Sound::loop(*hard_music_sample);
	}
	else {
		bpm = 60.0f / 75.0f; // (60 / BPM) BPM of taiko is actually 150 but its got a half time feel
		timer = bpm;
		timing_tolerance = bpm / 8.0f;
		music_loop = Sound::loop(*easy_music_sample);
	}

	// Reset grid variables
	grid_flash_duration = bpm / 4.0f; // how long grid color flashes last
	grid_timer = grid_flash_duration;

	// Reset message variables
	message_offset = 0.1f;
	cur_message_ind = 0;
	message_speed = bpm * 2.0f;

	reset_heart();
	
	sleeping = false;
	initialize_player_stats(is_hard_mode);
	game_state = game;
}

void PlayMode::setup_menu() {

	reset_heart();

	if (music_loop) {
		music_loop->stop();
	}
	music_loop = Sound::loop(*menu_music_sample);
	game_state = menu;
}

void PlayMode::to_death_screen() {

	deaths++;

	cur_heart->rotation = heart_base_rotation;
	cur_heart->scale = glm::vec3(1, 1, 1);

	if (music_loop) {
		music_loop->stop();
	}
	music_loop = Sound::loop(*death_music_sample);
	game_state = dead;
}