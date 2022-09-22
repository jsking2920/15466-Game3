#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <algorithm>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	// Functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	// Update functions based on game state
	void game_update(float elapsed);
	void menu_update(float elapsed);
	void death_update(float elapsed);
	// Draw functions based on game state
	void game_draw_ui(glm::uvec2 const& drawable_size);
	void menu_draw_ui(glm::uvec2 const& drawable_size);
	void death_draw_ui(glm::uvec2 const& drawable_size);

	//----- game state -----
	enum GameState {
		menu, game, dead
	};
	GameState game_state;

	// Input tracking
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} space, g, e, d, s, one, two, esc;

	// Local copy of the game scene (so code can change it during gameplay)
	Scene scene;

	// Hearts
	Scene::Transform* cur_heart = nullptr;
	Scene::Transform *good_heart = nullptr;
	Scene::Transform* mid_heart = nullptr;
	Scene::Transform* bad_heart = nullptr;
	glm::vec3 heart_base_pos;
	glm::quat heart_base_rotation;
	glm::vec3 heart_hidden_pos = glm::vec3(10, 10, 10);

	// Music + Beat Detection (all initialized in start_new_round based on difficulty)
	float bpm; 
	float timer; // Timer counts down from bpm, player tries to input on or near "0"
	std::shared_ptr< Sound::PlayingSample > music_loop;
	float timing_tolerance;

	// Player stats
	enum StatStatus {
		zero = 0, poor = 1, okay = 2, good = 3
	};
	struct PlayerStat {
		int8_t max; 
		int8_t cur; // 10/10: full, not thirst, not tired, etc.  0/10: starved, exhausted, dead
		StatStatus status;

		void update_cur(int8_t offset) {
			cur = clmp(cur + offset, int8_t(0), max);
			update_status();
		}

		void update_status() {
			if (cur == 0) {
				status = zero;
			}
			else if (cur <= (max / 3)) {
				status = poor;
			}
			else if (cur <= 2 * (max / 3)) {
				status = okay;
			}
			else {
				status = good;
			}
		}
	} hunger, thirst, food, water, fatigue;

	// Timing stats
	uint16_t missed_beats;
	uint16_t hits;
	uint16_t misses; // Off time clicks
	
	// Camera
	Scene::Camera *camera = nullptr;

	// Grid
	enum GridState {
		positive, negative, prompt, neutral
	};
	GridState grid_state = neutral;
	glm::u8vec4 grid_color = glm::u8vec4(0xff);
	float grid_flash_duration; // how long grid color flashes last
	float grid_timer;

	// Scrolling text
	float message_offset;
	float message_speed;
	glm::vec3* message_anchor_out = new glm::vec3();
	uint8_t cur_message_ind;
	std::vector<std::string> messages = { "First Message, this is the first message", "Yep, this is the second message", "Woo hoo third message" };

	// Helper Functions
	StatStatus get_overall_health();
	glm::u8vec4 get_stat_text_color(PlayerStat stat);
	void swap_heart(Scene::Transform* new_heart);
	void reset_heart();

	void initialize_player_stats(bool is_hard_mode);
	void setup_new_round(bool is_hard_mode);
	void setup_menu();
	void to_death_screen();
	

	static int8_t clmp(int8_t v, int8_t lo, int8_t hi) {
		if (v < lo) {
			return lo;
		}
		if (v > hi) {
			return hi;
		}
		return v;
	}
};
