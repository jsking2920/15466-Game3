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
	return new Sound::Sample(data_path("audio/TaikoLoop.opus"));
});

PlayMode::PlayMode() : scene(*main_scene) {
	// Get pointers to hearts
	for (auto &transform : scene.transforms) {
		std::cout << transform.name << std::endl;
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
	} 
	else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_SPACE) {
			space.pressed = false;
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
		// Flash grid red
		grid_timer = grid_flash_duration;
		grid_state = negative;
	}

	// Check for on-beat input
	if (space.downs == 1) {
		if (timer >= -timing_tolerance && timer <= timing_tolerance + elapsed) {
			timer = bpm + timer; // reset timer and account for error within tolerance window
			hits++;
			// Flash grid green
			grid_timer = grid_flash_duration;
			grid_state = positive;
		}
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

	// Roate Heart
	cur_heart->rotation *= glm::angleAxis(glm::radians(elapsed * 15.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	// Reset button press counters:
	space.downs = 0;
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

		//Scrolling Text
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.08f; // Specifies text height??
		lines.draw_text(messages[cur_message_ind],
			glm::vec3(aspect - message_offset * H, -1.0 + 0.45f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text(messages[cur_message_ind],
			glm::vec3(aspect - message_offset * H + ofs, -1.0 + 0.45f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00),
			message_anchor_out);

		// Check to see if the rightmost edge of the text has scrolled off the left side of the screen
		if (message_anchor_out->x < -aspect) {
			// Set next message, loop back to beginning if neccesary
			cur_message_ind = (cur_message_ind + 1) % messages.size();
			// Set text so that lest edge of new leftmost character is just off the right side of the screen
			message_offset = 0;
		}

		// Stats text
		lines.draw_text("Hits : " + std::to_string(hits),
			glm::vec3(0.35f * aspect, (-H * aspect) + 0.1f, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		lines.draw_text("Missed beats : " + std::to_string(missed_beats),
			glm::vec3(0.35f * aspect, (-2 * H * aspect) + 0.1f, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		lines.draw_text("Misses : " + std::to_string(misses),
			glm::vec3(0.35f * aspect, (-3 * H * aspect) + 0.1f, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		
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
		grid.draw(glm::vec3(-2, 0.997f, 0), glm::vec3(2, 0.997f, 0), grid_color);
		grid.draw(glm::vec3(-2, -0.997f, 0), glm::vec3(2, -0.997f, 0), grid_color);
		grid.draw(glm::vec3(-0.998f, 2, 0), glm::vec3(-0.998f, -2, 0), grid_color);
		grid.draw(glm::vec3(0.998f, 2, 0), glm::vec3(0.998f, -2, 0), grid_color);
	}
	GL_ERRORS();
}