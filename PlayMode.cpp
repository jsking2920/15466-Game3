#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint heart_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > heart_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("DanceOrDie.pnct"));
	heart_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > main_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("DanceOrDie.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
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

Load< Sound::Sample > normal_music_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("TaikoLoop.opus"));
});

Load< Sound::Sample > negative_sfx_sample(LoadTagDefault, []() -> Sound::Sample const* {
	return new Sound::Sample(data_path("NegativeSFX.opus"));
});

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

	// Set good heart as current heart
	cur_heart = good_heart;
	hearth_base_pos = good_heart->position;
	heart_base_rotation = good_heart->rotation;
	mid_heart->position += glm::vec3(10, 10, 10); // Hide off screen
	bad_heart->position += glm::vec3(10, 10, 10);

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	// Start music loop playing:
	music_loop = Sound::loop(*normal_music_sample);
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
	} 

	return false;
}

void PlayMode::update(float elapsed) {

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
		missed_beats++;

		hunger = std::clamp(--hunger, int8_t(0), max_hunger);
		thirst = std::clamp(--thirst, int8_t(0), max_thirst);
		fatigue = std::clamp(--fatigue, int8_t(0), max_fatigue);

		// Flash grid red
		grid_timer = grid_flash_duration;
		grid_state = negative;
	}

	// Check for on-beat input
	if (g.downs == 1 || e.downs == 1) {
		// input on time
		if (timer >= -timing_tolerance && timer <= timing_tolerance + elapsed) {
			timer = bpm + timer; // reset timer and account for error within tolerance window
			hits++;
			// Flash grid green
			grid_timer = grid_flash_duration;
			grid_state = positive;

			// Handle key specific logic
			// Gather
			if (g.downs == 1) {
				if (std::rand() % 2 == 0) {
					food++;
				}
				else {
					water++;
				}
			}
			// Eat
			if (e.downs == 1) {
				if (food > 0) {
					food--;
					hunger = std::clamp(++hunger, int8_t(0), max_hunger);
				}
				else {
					Sound::play(*negative_sfx_sample);
				}
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
	message_offset += 0.005f;

	// Rotate Heart
	cur_heart->rotation *= glm::angleAxis(glm::radians(elapsed * 18.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	// Beat heart in time
	float lerp_t = ((bpm - timer) / bpm);
	float sin = std::sin(lerp_t * float(M_PI));
	float scalar = (std::abs(sin) * 0.2f) + 0.8f;
	cur_heart->scale = glm::vec3(scalar);

	// Reset button press counters:
	space.downs = 0;
	g.downs = 0;
	e.downs = 0;
	d.downs = 0;
	s.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{
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

		// Timing stats text
		lines.draw_text("Hits : " + std::to_string(hits),
			glm::vec3(0.35f * aspect, (-H2 * aspect) + 0.1f, 0.0),
			glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		lines.draw_text("Missed beats : " + std::to_string(missed_beats),
			glm::vec3(0.35f * aspect, (-2 * H2 * aspect) + 0.1f, 0.0),
			glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		lines.draw_text("Misses : " + std::to_string(misses),
			glm::vec3(0.35f * aspect, (-3 * H2 * aspect) + 0.1f, 0.0),
			glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0xff));

		// Player stats text
		lines.draw_text("Hunger",
			glm::vec3((-3.0f / 4.0f) * aspect - 0.07f, (H1 * aspect) + 0.25f, 0.0),
			glm::vec3(H1, 0.0f, 0.0f), glm::vec3(0.0f, H1, 0.0f),
			get_stat_text_color(hunger, max_hunger));
		lines.draw_text(std::to_string(hunger) + "/" + std::to_string(max_hunger),
			glm::vec3((-3.0f / 4.0f) * aspect + 0.08f, (H2 * aspect) + 0.23f, 0.0),
			glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
			get_stat_text_color(hunger, max_hunger));
		lines.draw_text("[ E ] at",
			glm::vec3((-3.0f / 4.0f) * aspect + 0.06f, (H2 * aspect) + 0.05f, 0.0),
			glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0xff));

		lines.draw_text("Food",
			glm::vec3((-3.0f / 4.0f) * aspect - 0.02f, (H1 * -aspect) + 0.05f, 0.0),
			glm::vec3(H1, 0.0f, 0.0f), glm::vec3(0.0f, H1, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		lines.draw_text(std::to_string(food),
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
			get_stat_text_color(thirst, max_thirst));
		lines.draw_text(std::to_string(thirst) + "/" + std::to_string(max_thirst),
			glm::vec3(-0.09f, (H2 * aspect) + 0.23f, 0.0),
			glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
			get_stat_text_color(thirst, max_thirst));
		lines.draw_text("[ D ] rink",
			glm::vec3(-0.13f, (H2 * aspect) + 0.05f, 0.0),
			glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0xff));

		lines.draw_text("Water",
			glm::vec3(-0.16, (H1 * -aspect) + 0.05f, 0.0),
			glm::vec3(H1, 0.0f, 0.0f), glm::vec3(0.0f, H1, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		lines.draw_text(std::to_string(water),
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
			get_stat_text_color(fatigue, max_fatigue));
		lines.draw_text(std::to_string(fatigue) + "/" + std::to_string(max_fatigue),
			glm::vec3((1.0f / 2.0f) * aspect + 0.20f, (H2 * aspect) + 0.23f, 0.0),
			glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
			get_stat_text_color(fatigue, max_fatigue));
		lines.draw_text("[ S ] leep",
			glm::vec3((1.0f / 2.0f) * aspect + 0.16f, (H2 * aspect) + 0.05f, 0.0),
			glm::vec3(H2, 0.0f, 0.0f), glm::vec3(0.0f, H2, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0xff));
	}
	GL_ERRORS();
}

glm::u8vec4 PlayMode::get_stat_text_color(uint16_t cur, uint16_t max) {
	if (cur <= (max / 3)) {
		return glm::u8vec4(0xff, 0x99, 0x99, 0xff);
	} 
	else if (cur <= 2 * (max / 3)) {
		return glm::u8vec4(0xff, 0xff, 0xff, 0xff);
	}
	else {
		return glm::u8vec4(0x99, 0xff, 0x99, 0xff);
	}
}