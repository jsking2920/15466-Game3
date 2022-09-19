#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	// Functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	// Input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} space;

	// Local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// Hearts
	Scene::Transform* cur_heart = nullptr;
	Scene::Transform *good_heart = nullptr;
	Scene::Transform* mid_heart = nullptr;
	Scene::Transform* bad_heart = nullptr;
	glm::vec3 hearth_base_pos;
	glm::quat heart_base_rotation;

	// Music + Beat Detection
	std::shared_ptr< Sound::PlayingSample > music_loop;
	float bpm = 60.0f / 75.0f; // (60 / BPM) BPM of taiko is actually 150 but its got a half time feel
	float timer = bpm; // Timer counts down from bpm, player tries to input on or near "0"
	float timing_tolerance = bpm / 8.0f; // Can miss by up to an eighth of a beat and still count as a hit

	// Player stats

	// Timing stats
	uint16_t missed_beats = 0;
	uint16_t hits = 0;
	uint16_t misses = 0; // Off time clicks
	
	// Camera
	Scene::Camera *camera = nullptr;

	// Grid
	enum GridState {
		positive, negative, prompt, neutral
	};
	GridState grid_state = neutral;
	glm::u8vec4 grid_color = glm::u8vec4(0xff);
	float grid_flash_duration = bpm / 4.0f; // how long grid color flashes last
	float grid_timer = grid_flash_duration;

	// Scrolling text
	float message_offset = 0.1f;
	glm::vec3 * message_anchor_out = new glm::vec3();
	uint8_t cur_message_ind = 0;
	std::vector<std::string> messages = {"First Message, this is the first message", "Yep, this is the second message", "Woo hoo third message"};
};
