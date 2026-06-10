# Certified Model Improvement Plan

## Goal
Improve certified model accuracy from 76.1% to 90%+ to enable statistical confidence in champion ranking validation.

## Current State
- **Model:** Logistic regression on 6 features (base, synergy, counter for both teams)
- **Accuracy:** 76.1% test accuracy on draft_ceiling_holdout_5000.csv
- **MSE:** 0.0484
- **Features:** Simple averages of pairwise winrates
- **Implementation:** Baked weights in native/src/simulation/sim_draft_recommender.cpp (lines 436-513)
- **Training Data:** 5000 compositions from draft_ceiling_holdout_5000.csv

## Available but Unused Features

### 1. Champion Stats (15+ fields from ChampionStats)
- max_hp, attack_damage, attack_range, attack_speed, move_speed
- armor, magic_resist, tenacity, life_steal
- mana_cost, ability_cd, projectile_speed, projectile_radius

### 2. Effect Kinds (50+ from EffectSpec and SimConstants)
- CC types: stun, silence, root, taunt, disarm, slow, knockback
- Damage types: physical, magic, true
- Utility: shield, heal, dodge, stealth, dash, summon
- Advanced: reflect, hp_threshold_damage_multiplier, distance_threshold_multiplier

### 3. Mechanical Signals (already computed in MECHANICAL_COLUMNS)
- estimated_dps, effective_hp, burst_estimate, sustain_per_sec
- cc_per_sec, attack_range, max_aoe_radius, ability_uptime

### 4. Advanced Features (computed but unused)
- Variance: synergy_variance, counter_variance (from RelationshipAggregate)
- Specificity: best_counter, worst_counter, best_synergy, worst_synergy
- Role information: tank, fighter, assassin, marksman, mage, support

## Phase 1: Quick Wins (Expected: 76% → 80% accuracy)

### Phase 1.1: Add variance and specificity features to certified model

**Implementation Details:**
- File: `native/src/simulation/sim_draft_recommender.cpp`
  - Extend `CertifiedPairwiseFeatures` struct (currently lines 93-97):
    ```cpp
    struct CertifiedPairwiseFeatures {
        double base = 0.5;
        double synergy = 0.5;
        double counter = 0.5;
        // New fields:
        double synergy_variance = 0.0;
        double counter_variance = 0.0;
        double best_counter = 0.5;
        double worst_counter = 0.5;
        double best_synergy = 0.5;
        double worst_synergy = 0.5;
    };
    ```

- File: `native/src/simulation/sim_draft_recommender.cpp`
  - Modify `extract()` function in `calculate_certified_pairwise_probability()` (lines 475-502)
  - Use `RelationshipAggregate` which already computes variance and min/max
  - Update feature array from 6 to 12 elements
  - Update weights, means, stddevs arrays from size 6 to 12

- File: `scripts/tools/verify_pairwise_signal.gd`
  - Modify `_extract_pairwise_team_features()` (lines 335-359)
  - Add variance and specificity extraction
  - Add helper functions: `_compute_variance()`, `_compute_max()`, `_compute_min()`

**Verification:**
- Run: `godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output/draft_ceiling_holdout_5000.csv --feature-sets=pairwise_extended`
- Compare test accuracy against baseline (76.1%)
- **Target:** 77%+ accuracy
- **Gate:** If target met → proceed to Phase 1.2

### Phase 1.2 (Deferred): Add champion stat averages to certified model

**Status:** Deferred. Champion base stats (`max_hp`, `attack_damage`, etc.) live in `champion_schema.json` and the GDScript `ChampionCatalog`, but `DraftStatsDatabase` (C++) only loads `combat_stats.csv`, which contains simulation-derived outcome stats (`avg_dmg_dealt`, `avg_healing`, etc.). Using outcome stats would leak the label. Loading base stats from JSON requires new C++ infrastructure. This phase is deferred pending a future catalog integration pass.

If needed later:
- Extend `CertifiedPairwiseFeatures` with `avg_hp`, `avg_damage`, `avg_range`, `avg_armor`, `avg_mr`, `avg_speed`
- Add champion stat extraction in `extract()` using champion catalog
- Update feature array from 12 to 24 elements

**Gate:** Proceed directly to Phase 1.2 (role balance) after Phase 1.1.

### Phase 1.2: Add role balance features to certified model

**Implementation Details:**
- File: `native/src/simulation/sim_draft_recommender.cpp`
  - Extend `CertifiedPairwiseFeatures` struct:
    ```cpp
    // Role counts per team
    int tank_count = 0;
    int fighter_count = 0;
    int assassin_count = 0;
    int marksman_count = 0;
    int mage_count = 0;
    int support_count = 0;
    ```

- File: `native/src/simulation/sim_draft_recommender.cpp`
  - Add role extraction logic in `extract()` function
  - Use `get_champion_role()` from DraftStatsDatabase
  - Count roles per team
  - Update feature array from 12 to 24 elements (6 roles per team)
  - Update weights, means, stddevs arrays to size 24

- File: `scripts/tools/verify_pairwise_signal.gd`
  - Add role extraction to `_extract_pairwise_team_features()`
  - Load role information from champion catalog

**Verification:**
- Run: `godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output/draft_ceiling_holdout_5000.csv --feature-sets=pairwise_extended_roles`
- Compare test accuracy against Phase 1.1
- **Target:** 80%+ accuracy
- **Gate:** If target met → proceed to Phase 1.3

### Phase 1.3: Train and verify Phase 1 model improvements

**Implementation Details:**
- File: `scripts/tools/verify_pairwise_signal.gd`
  - Add new feature set "pairwise_phase1" combining variance, specificity, and role features
  - Train logistic regression on extended feature set
  - Generate new weights, means, stddevs

- File: `native/src/simulation/sim_draft_recommender.cpp`
  - Update baked weights in `calculate_certified_pairwise_probability()` (lines 439-463)
  - Update array sizes from 6 to 24
  - Update feature extraction to match new feature set

**Verification:**
- Run: `godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output/draft_ceiling_holdout_5000.csv --feature-sets=pairwise_phase1 --split-repeats=5`
- Measure: test accuracy, MSE, calibration
- **Target:** 80%+ test accuracy, improved MSE (<0.04)
- **Gate:** If target met → proceed to Phase 2
- **Fallback:** If target not met → analyze feature importance, remove low-impact features

## Phase 2: Medium Effort (Expected: 80% → 85% accuracy)

### Phase 2.1: Add effect kind features to certified model

**Implementation Details:**
- File: `scripts/tools/verify_pairwise_signal.gd`
  - Create `_extract_effect_kinds()` function
  - Parse champion effect trees using champion catalog
  - Count effect kinds per champion:
    ```gdscript
    func _extract_effect_kinds(champion_id: StringName) -> Dictionary:
        var champion := ChampionCatalogScript.get_champion_spec(champion_id)
        var effect_counts := {
            "cc_duration": 0.0,
            "damage_physical": 0,
            "damage_magic": 0,
            "damage_true": 0,
            "utility_count": 0,
            "sustain_count": 0,
            "mobility_count": 0,
        }
        # Parse effect tree and count kinds
        return effect_counts
    ```

- File: `native/src/simulation/sim_draft_recommender.cpp`
  - Extend `CertifiedPairwiseFeatures` struct:
    ```cpp
    // Effect kind counts per team
    int cc_duration = 0;
    int damage_physical = 0;
    int damage_magic = 0;
    int damage_true = 0;
    int utility_count = 0;
    int sustain_count = 0;
    int mobility_count = 0;
    ```

- File: `native/src/simulation/sim_draft_recommender.cpp`
  - Add effect kind extraction in `extract()` function
  - Sum effect counts per team, normalize by team size
  - Update feature array from 24 to 38 elements (7 effects per team)

**Verification:**
- Run: `godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output/draft_ceiling_holdout_5000.csv --feature-sets=pairwise_phase2_effects`
- Compare test accuracy against Phase 1.3
- **Target:** 82%+ accuracy
- **Gate:** If target met → proceed to Phase 2.2

### Phase 2.2: Increase training data size

**Implementation Details:**
- File: `scripts/tools/stats_simulation_csv_generator.gd`
  - Run additional simulation batches
  - Target: 10,000+ compositions (double current dataset)
  - Command: `godot --headless --script res://scripts/tools/stats_simulation_csv_generator.gd -- --matches=10000 --team-sizes=5 --output-dir=res://stats_output_expanded`
  - Generate new `draft_ceiling_holdout_10000.csv`

- File: `scripts/tools/measure_draft_ceiling.gd`
  - Run ceiling measurement on expanded dataset
  - Command: `godot --headless --script res://scripts/tools/measure_draft_ceiling.gd -- --input-dir=res://stats_output_expanded --output=res://stats_output_expanded/draft_ceiling_expanded.csv`

**Verification:**
- Train model on expanded dataset
- Compare test accuracy on holdout set
- Target: Improved generalization, reduced overfitting
- **Target:** 83%+ accuracy
- **Gate:** If target met → proceed to Phase 2.3

### Phase 2.3: Add feature engineering (interactions)

**Implementation Details:**
- File: `scripts/tools/verify_pairwise_signal.gd`
  - Add interaction feature extraction:
    ```gdscript
    func _extract_interaction_features(base: float, synergy: float, counter: float) -> Dictionary:
        return {
            "base_synergy": base * synergy,
            "synergy_counter": synergy * counter,
            "base_counter": base * counter,
            "base_squared": base * base,
            "synergy_squared": synergy * synergy,
            "counter_squared": counter * counter,
        }
    ```
  - Add team differential features: team1_stats - team2_stats

- File: `native/src/simulation/sim_draft_recommender.cpp`
  - Extend `CertifiedPairwiseFeatures` struct with interaction fields
  - Add 6 interaction terms per team (12 total)

- File: `native/src/simulation/sim_draft_recommender.cpp`
  - Add interaction computation in `extract()` function
  - Update feature array from 50 to 74 elements

**Verification:**
- Run: `godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output_expanded/draft_ceiling_holdout_10000.csv --feature-sets=pairwise_phase2_interactions`
- Compare test accuracy against Phase 2.2
- **Target:** 85%+ accuracy
- **Gate:** If target met → proceed to Phase 2.4

### Phase 2.4: Train and verify Phase 2 model improvements

**Implementation Details:**
- File: `scripts/tools/verify_pairwise_signal.gd`
  - Add "pairwise_phase2" feature set combining all Phase 2 features
  - Train logistic regression on extended feature set
  - Implement L2 regularization to prevent overfitting
  - Generate new weights, means, stddevs

- File: `native/src/simulation/sim_draft_recommender.cpp`
  - Update baked weights for 74 features
  - Update feature extraction to match Phase 2 feature set

**Verification:**
- Run: `godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output_expanded/draft_ceiling_holdout_10000.csv --feature-sets=pairwise_phase2 --split-repeats=5`
- Measure: test accuracy, MSE, calibration, feature importance
- **Target:** 85%+ test accuracy, significantly improved MSE (<0.03)
- **Gate:** If target met → proceed to Phase 3
- **Fallback:** If target not met → increase regularization, try feature selection

## Phase 3: Advanced (Expected: 85% → 90%+ accuracy)

### Phase 3.1: Implement neural network model

**Implementation Details:**
- File: `scripts/tools/verify_pairwise_signal.gd`
  - Implement simple neural network in GDScript:
    ```gdscript
    class SimpleNeuralNetwork:
        var weights1: Array[Array]  # Hidden layer weights
        var weights2: Array[Array]  # Output layer weights
        var bias1: Array[float]
        var bias2: Array[float]

        func forward(input: Array[float]) -> Array[float]:
            # Hidden layer with ReLU
            var hidden := _relu(_matmul(input, weights1) + bias1)
            # Output layer with sigmoid
            var output := _sigmoid(_matmul(hidden, weights2) + bias2)
            return output

        func train(inputs: Array, labels: Array, epochs: int, learning_rate: float):
            # Backpropagation implementation
            pass
    ```
  - Architecture: Input (74) → Hidden (32) → Hidden (16) → Output (1)
  - Activation: ReLU for hidden, sigmoid for output
  - Loss: Binary cross-entropy
  - Optimizer: SGD with momentum

**Verification:**
- Train NN on Phase 2 feature set
- Compare NN accuracy against logistic regression baseline
- **Target:** 87%+ accuracy
- Analyze overfitting, implement dropout if needed
- **Gate:** If target met → proceed to Phase 3.2

### Phase 3.2: Implement ensemble methods

**Implementation Details:**
- File: `scripts/tools/verify_pairwise_signal.gd`
  - Implement multiple models with different feature subsets:
    - Model 1: Logistic regression on all features
    - Model 2: Neural network on all features
    - Model 3: Logistic regression on top 20 features (feature selection)
    - Model 4: Neural network on top 20 features
  - Create ensemble prediction:
    ```gdscript
    func ensemble_predict(features: Dictionary) -> float:
        var predictions := []
        predictions.append(model1.predict(features))
        predictions.append(model2.predict(features))
        predictions.append(model3.predict(features))
        predictions.append(model4.predict(features))
        return _weighted_average(predictions, [0.3, 0.3, 0.2, 0.2])
    ```

**Verification:**
- Compare ensemble accuracy against single models
- **Target:** 88%+ accuracy
- Analyze feature importance across ensemble
- **Gate:** If target met → proceed to Phase 3.3

### Phase 3.3: Hyperparameter optimization

**Implementation Details:**
- File: `scripts/tools/verify_pairwise_signal.gd`
  - Implement grid search for hyperparameters:
    ```gdscript
    func grid_search():
        var learning_rates := [0.001, 0.01, 0.1]
        var hidden_sizes := [[16], [32], [32, 16], [64, 32]]
        var regularization := [0.0, 0.001, 0.01]
        var best_accuracy := 0.0
        var best_params := {}

        for lr in learning_rates:
            for hidden in hidden_sizes:
                for reg in regularization:
                    var accuracy := _train_and_evaluate(lr, hidden, reg)
                    if accuracy > best_accuracy:
                        best_accuracy = accuracy
                        best_params = {lr=lr, hidden=hidden, reg=reg}
        return best_params
    ```
  - Use 5-fold cross-validation on training set
  - Optimize: learning rate, hidden layer sizes, regularization, ensemble weights

**Verification:**
- Compare optimized model against baseline
- **Target:** 90%+ accuracy
- Ensure no overfitting on validation set
- **Gate:** If target met → proceed to Phase 3.4

### Phase 3.4: Final training and verification

**Implementation Details:**
- File: `scripts/tools/verify_pairwise_signal.gd`
  - Train final optimized model on full dataset
  - Generate production-ready model weights/parameters
  - Export model parameters to native code format

- File: `native/src/simulation/sim_draft_recommender.cpp`
  - Update `calculate_certified_pairwise_probability()` with new model
  - If using NN: Implement forward pass in C++
  - If using ensemble: Implement weighted averaging in C++
  - Update model parameters (weights, biases)

**Verification:**
- Run comprehensive validation on holdout set
- Measure: accuracy, MSE, calibration, feature importance
- **Target:** 90%+ test accuracy, excellent calibration
- Test rollout validation with improved ground truth:
  - Run: `godot --headless --script res://scripts/tools/validate_pick_recommendations.gd -- --states=100 --rollouts-per-candidate=100`
  - Measure ranking confidence intervals
  - Verify statistical confidence in champion rankings
- **Final Gate:** 90%+ accuracy → Success

## Success Criteria

### Overall Goal
90%+ test accuracy on certified model

### Phase Gates
- **Phase 1 Gate:** 80%+ accuracy → Proceed to Phase 2
- **Phase 2 Gate:** 85%+ accuracy → Proceed to Phase 3
- **Phase 3 Gate:** 90%+ accuracy → Success

### Fallback Options
- **If Phase 1 fails (<80%):** Analyze feature importance, remove low-impact features, try different feature combinations
- **If Phase 2 fails (<85%):** Increase data size further (20000+ compositions), try different feature engineering, increase regularization
- **If Phase 3 fails (<90%):** Accept 85-90% as practical limit, focus on A/B testing for production decisions

### Final Validation
- Once 85%+ accuracy achieved: Test rollout validation reliability
- Measure ranking confidence intervals using bootstrap sampling
- Verify statistical confidence in champion rankings
- Update wiki with final results and limitations

## Files to Modify

### Native Code
- `native/src/simulation/sim_draft_recommender.cpp`
  - Extend CertifiedPairwiseFeatures struct (lines 93-97) with variance, specificity, and role fields
  - Update calculate_certified_pairwise_probability() (lines 436-513)
  - Modify extract() function to compute new features
  - Update baked weights, means, stddevs arrays (6 → 24 features)
  - Implement new model architecture (NN/ensemble) in Phase 3

### GDScript Tools
- `scripts/tools/verify_pairwise_signal.gd`
  - Add variance/specificity extraction (_compute_variance, _compute_max, _compute_min)
  - Add role count extraction
  - Add new feature sets: pairwise_extended, pairwise_extended_roles, pairwise_phase1
  - Implement neural network class (Phase 3)
  - Implement ensemble methods (Phase 3)
  - Implement hyperparameter optimization (Phase 3)

- `scripts/tools/stats_simulation_csv_generator.gd`
  - Use for generating expanded training data (Phase 2.2)

- `scripts/tools/measure_draft_ceiling.gd`
  - Use for measuring ceiling on expanded dataset (Phase 2.2)

## Dependencies
- Existing pairwise winrate data (combat_stats.csv, matchup_with.csv, matchup_vs.csv)
- Draft ceiling data (draft_ceiling_holdout_5000.csv)
- Champion catalog with effect trees and stats
- RelationshipAggregate (already computes variance and specificity)

## Verification Commands

### Phase 1 Verification
```bash
# Phase 1.1
godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output/draft_ceiling_holdout_5000.csv --feature-sets=pairwise_extended

# Phase 1.2
godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output/draft_ceiling_holdout_5000.csv --feature-sets=pairwise_extended_roles

# Phase 1.3
godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output/draft_ceiling_holdout_5000.csv --feature-sets=pairwise_phase1 --split-repeats=5
```

### Phase 2 Verification
```bash
# Phase 2.1
godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output/draft_ceiling_holdout_5000.csv --feature-sets=pairwise_phase2_effects

# Phase 2.2
godot --headless --script res://scripts/tools/stats_simulation_csv_generator.gd -- --matches=10000 --team-sizes=5 --output-dir=res://stats_output_expanded
godot --headless --script res://scripts/tools/measure_draft_ceiling.gd -- --input-dir=res://stats_output_expanded --output=res://stats_output_expanded/draft_ceiling_expanded.csv

# Phase 2.3
godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output_expanded/draft_ceiling_holdout_10000.csv --feature-sets=pairwise_phase2_interactions

# Phase 2.4
godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output_expanded/draft_ceiling_holdout_10000.csv --feature-sets=pairwise_phase2 --split-repeats=5
```

### Phase 3 Verification
```bash
# Phase 3.1
godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output_expanded/draft_ceiling_holdout_10000.csv --feature-sets=pairwise_phase3_nn

# Phase 3.2
godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output_expanded/draft_ceiling_holdout_10000.csv --feature-sets=pairwise_phase3_ensemble

# Phase 3.3
godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output_expanded/draft_ceiling_holdout_10000.csv --feature-sets=pairwise_phase3_optimized

# Phase 3.4
godot --headless --script res://scripts/tools/verify_pairwise_signal.gd -- --ceiling-input=res://stats_output_expanded/draft_ceiling_holdout_10000.csv --feature-sets=pairwise_phase3_final --split-repeats=10
```

### Final Validation
```bash
# Test rollout validation with improved ground truth
godot --headless --script res://scripts/tools/validate_pick_recommendations.gd -- --states=100 --rollouts-per-candidate=100 --draft-depth=4
```
