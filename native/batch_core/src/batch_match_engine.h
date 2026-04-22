#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace teamfight {

struct UnitState {
  int32_t instance_id = 0;
  std::string hero_id;
  std::string display_name;
  std::string role;
  std::string team;
  std::string passive_id;
  bool alive = true;
  float hp = 0.0f;
  float shield = 0.0f;
  float position_x = 0.0f;
  float position_y = 0.0f;
  int32_t target_id = -1;
  bool in_range = false;
  int32_t kills = 0;
  int32_t deaths = 0;
  int32_t assists = 0;
  float damage_dealt = 0.0f;
  float damage_dealt_auto = 0.0f;
  float damage_dealt_ability = 0.0f;
  float damage_dealt_ultimate = 0.0f;
  float damage_received = 0.0f;
  float damage_mitigated = 0.0f;
  float healing_done = 0.0f;
  float shielding_done = 0.0f;
  int32_t auto_attacks_done = 0;
  int32_t abilities_used = 0;
  int32_t ultimates_used = 0;
  int32_t stuns_dealt = 0;
  float max_hp = 1.0f;
  float attack_damage = 0.0f;
  float attack_range = 0.0f;
  float attack_speed = 1.0f;
  float move_speed = 0.0f;
  float armor = 0.0f;
  float projectile_speed = 0.0f;
  float magic_resist = 0.0f;
  float tenacity = 0.0f;
  float life_steal = 0.0f;
  float respawn_time = 0.0f;
  float mana_per_attack = 0.0f;
  float max_mana = 0.0f;
  float mana = 0.0f;
  float ability_cd = 0.0f;
  float ultimate_cd = 0.0f;
  float ability_cooldown_remaining = 0.0f;
  float ultimate_cooldown_remaining = 0.0f;
  float projectile_radius = 0.0f;
  float attack_cooldown_remaining = 0.0f;
  float stun_remaining = 0.0f;
  int32_t current_target_id = -1;
  float retarget_timer = 0.0f;
  float target_switch_lock_timer = 0.0f;
  float last_target_score = 0.0f;
};

struct ProjectileState {
  bool active = false;
  int32_t owner_id = -1;
  int32_t target_id = -1;
  float damage = 0.0f;
  float remaining_time = 0.0f;
};

struct EffectState {
  bool active = false;
  int32_t owner_id = -1;
  int32_t target_id = -1;
  std::string kind;
  float remaining_time = 0.0f;
  int32_t stacks = 0;
};

struct TargetState {
  int32_t unit_id = -1;
  int32_t target_id = -1;
  float score = 0.0f;
  bool in_range = false;
};

struct MatchResult {
  std::string termination;
  std::string winner;
  int32_t player_kills = 0;
  int32_t enemy_kills = 0;
  float time = 0.0f;
  int32_t ticks = 0;
};

class BatchMatchEngine {
public:
  void set_seed(int32_t seed);
  void set_units(const std::vector<UnitState>& units);
  void step(float dt);
  MatchResult get_match_result() const;
  std::vector<UnitState> snapshot_units() const;
  void clear();

private:
  void resolve_step(float dt);
  int32_t find_nearest_enemy_index(int32_t unit_index) const;
  int32_t select_target_index(int32_t unit_index) const;
  float score_target(const UnitState &unit, const UnitState &enemy) const;
  float distance_weight_for_role(const std::string &role) const;
  float hp_weight_for_role(const std::string &role) const;
  float stickiness_for_role(const std::string &role) const;
  float in_range_bonus_for_role(const std::string &role) const;
  float tank_penalty_for_role(const std::string &role) const;
  void move_toward_target(UnitState &unit, const UnitState &target, float dt);
  void perform_attack(UnitState &attacker, UnitState &target);
  void perform_ability(UnitState &attacker, UnitState &target, bool is_ultimate);
  void finish_with_timeout();
  static float apply_armor(float damage, float armor);

  static constexpr float MATCH_DURATION = 60.0f;

  int32_t seed_ = 0;
  float time_ = 0.0f;
  int32_t ticks_ = 0;
  bool finished_ = false;
  std::string termination_;
  std::string winner_;
  int32_t player_kills_ = 0;
  int32_t enemy_kills_ = 0;
  std::vector<UnitState> units_;
  std::vector<ProjectileState> projectiles_;
  std::vector<EffectState> effects_;
  std::vector<TargetState> targets_;
};

} // namespace teamfight
