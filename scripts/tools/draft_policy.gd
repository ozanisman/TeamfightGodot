extends RefCounted
## Shared draft-AI selection policy. Extracted from simulation_viewer_base.gd so the
## validation harness can measure the exact stochastic policy shipped in gameplay.

static func softmax_select(recommendations: Array, temperature: float = 1.0, scale: float = 100.0, debug: bool = false) -> StringName:
	if temperature <= 0.0:
		push_error("DraftPolicy.softmax_select: temperature must be positive, got %f" % temperature)
		temperature = 0.01
	elif temperature > 100.0:
		push_warning("DraftPolicy.softmax_select: temperature unusually high (%f), clamping to 100.0" % temperature)
		temperature = 100.0

	var scores: Array[float] = []
	var champions: Array[StringName] = []
	for rec in recommendations:
		var score: float = rec.get("total_score", 0.0)
		var champion: StringName = StringName(rec.get("candidate", ""))
		if not champion.is_empty():
			scores.append(score)
			champions.append(champion)

	if champions.is_empty():
		return StringName()

	# Numerical stability: subtract max scaled score before exp
	var max_scaled: float = scores[0] * scale
	for score in scores:
		var scaled: float = score * scale
		if scaled > max_scaled:
			max_scaled = scaled

	var exp_values: Array[float] = []
	var exp_sum: float = 0.0
	for score in scores:
		var temp_score: float = (score * scale - max_scaled) / temperature
		var exp_val: float = exp(temp_score)
		exp_values.append(exp_val)
		exp_sum += exp_val

	var probabilities: Array[float] = []
	for exp_val in exp_values:
		probabilities.append(exp_val / exp_sum)

	if debug:
		print("  --- Softmax Details (temp=%.2f, scale=%.1f) ---" % [temperature, scale])
		for i in range(champions.size()):
			print("    #%d: %s | raw_score=%.4f | scaled=%.4f | exp_val=%.4f | prob=%.2f%%" % [i + 1, champions[i], scores[i], scores[i] * scale, exp_values[i], probabilities[i] * 100])

	var rand_val: float = randf()
	if debug:
		print("  Random sample: %.4f" % rand_val)
	var cumulative: float = 0.0
	for i in range(probabilities.size()):
		cumulative += probabilities[i]
		if rand_val <= cumulative:
			if debug:
				print("  Selected rank #%d (score: %.4f, prob: %.2f%%)" % [i + 1, scores[i], probabilities[i] * 100])
			return champions[i]

	return champions[champions.size() - 1]
