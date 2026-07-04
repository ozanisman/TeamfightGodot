extends RefCounted
## Elo rating engine for draft strategy self-play ladders.

const DEFAULT_INITIAL_RATING: float = 1500.0
const DEFAULT_K_FACTOR: float = 32.0
const CONVERGENCE_EPSILON: float = 0.01


static func expected_score(rating_a: float, rating_b: float) -> float:
	return 1.0 / (1.0 + pow(10.0, (rating_b - rating_a) / 400.0))


static func update_ratings(rating_a: float, rating_b: float, score_a: float, k: float) -> Dictionary:
	var expected_a: float = expected_score(rating_a, rating_b)
	var expected_b: float = 1.0 - expected_a
	var score_b: float = 1.0 - score_a
	return {
		"rating_a": rating_a + k * (score_a - expected_a),
		"rating_b": rating_b + k * (score_b - expected_b),
	}


static func pairing_blue_score_rate(p: Dictionary) -> float:
	var total: int = int(p["blue_wins"]) + int(p["red_wins"]) + int(p["draws"])
	if total <= 0:
		return 0.0
	return (float(p["blue_wins"]) + 0.5 * float(p["draws"])) / float(total)


static func stats_score_rate(s: Dictionary) -> float:
	var games: int = int(s["games"])
	if games <= 0:
		return 0.0
	return (float(s["wins"]) + 0.5 * float(s["draws"])) / float(games)


static func compute_ladder(
	drafts: Array,
	strategies: Array[String],
	initial_rating: float = DEFAULT_INITIAL_RATING,
	k_factor: float = DEFAULT_K_FACTOR,
	iterations: int = 10
) -> Dictionary:
	var ratings: Dictionary = {}
	var stats: Dictionary = {}
	for strategy in strategies:
		ratings[strategy] = initial_rating
		stats[strategy] = {"games": 0, "wins": 0, "losses": 0, "draws": 0}

	var pairings: Dictionary = _aggregate_pairings(drafts, strategies)
	if pairings.is_empty():
		return {"ratings": ratings, "stats": stats, "pairings": pairings}

	for _iter in range(maxi(1, iterations)):
		var prev_ratings: Dictionary = ratings.duplicate()
		for key in pairings.keys():
			var p: Dictionary = pairings[key]
			var blue: String = p["blue_strategy"]
			var red: String = p["red_strategy"]
			if blue == red:
				continue
			var total: int = p["blue_wins"] + p["red_wins"] + p["draws"]
			if total <= 0:
				continue
			var score_blue: float = pairing_blue_score_rate(p)
			var updated: Dictionary = update_ratings(ratings[blue], ratings[red], score_blue, k_factor)
			ratings[blue] = updated["rating_a"]
			ratings[red] = updated["rating_b"]

		var max_delta: float = 0.0
		for strategy in strategies:
			max_delta = maxf(max_delta, absf(ratings[strategy] - prev_ratings[strategy]))
		if max_delta < CONVERGENCE_EPSILON:
			break

	for key in pairings.keys():
		var p: Dictionary = pairings[key]
		var blue: String = p["blue_strategy"]
		var red: String = p["red_strategy"]
		if blue == red:
			continue
		var sims: int = int(p["blue_wins"]) + int(p["red_wins"]) + int(p["draws"])
		stats[blue]["games"] += sims
		stats[blue]["wins"] += p["blue_wins"]
		stats[blue]["losses"] += p["red_wins"]
		stats[blue]["draws"] += p["draws"]
		stats[red]["games"] += sims
		stats[red]["wins"] += p["red_wins"]
		stats[red]["losses"] += p["blue_wins"]
		stats[red]["draws"] += p["draws"]

	return {"ratings": ratings, "stats": stats, "pairings": pairings}


static func _aggregate_pairings(drafts: Array, strategies: Array[String]) -> Dictionary:
	var strategy_set: Dictionary = {}
	for strategy in strategies:
		strategy_set[strategy] = true

	var pairings: Dictionary = {}
	for draft in drafts:
		var blue: String = String(draft["blue_strategy"])
		var red: String = String(draft["red_strategy"])
		if not strategy_set.has(blue) or not strategy_set.has(red):
			continue
		var key: String = "%s|%s" % [blue, red]
		if not pairings.has(key):
			pairings[key] = {
				"blue_strategy": blue,
				"red_strategy": red,
				"trials": 0,
				"blue_wins": 0,
				"red_wins": 0,
				"draws": 0,
			}
		var p: Dictionary = pairings[key]
		p["trials"] += 1
		p["blue_wins"] += int(draft["blue_wins"])
		p["red_wins"] += int(draft["red_wins"])
		p["draws"] += int(draft["draws"])
	return pairings


static func run_self_test() -> bool:
	var dominant_wins: Array = [
		{
			"blue_strategy": "strong",
			"red_strategy": "weak",
			"blue_wins": 100,
			"red_wins": 0,
			"draws": 0,
		},
		{
			"blue_strategy": "weak",
			"red_strategy": "strong",
			"blue_wins": 0,
			"red_wins": 100,
			"draws": 0,
		},
	]
	var strategies: Array[String] = ["strong", "weak"]
	var ladder: Dictionary = compute_ladder(dominant_wins, strategies, DEFAULT_INITIAL_RATING, DEFAULT_K_FACTOR, 20)
	var ratings: Dictionary = ladder["ratings"]
	if ratings["strong"] <= ratings["weak"]:
		push_error("draft_elo_rating self-test: strong should outrank weak")
		return false

	var draw_only: Array = [
		{
			"blue_strategy": "a",
			"red_strategy": "b",
			"blue_wins": 0,
			"red_wins": 0,
			"draws": 100,
		},
		{
			"blue_strategy": "b",
			"red_strategy": "a",
			"blue_wins": 0,
			"red_wins": 0,
			"draws": 100,
		},
	]
	var draw_strategies: Array[String] = ["a", "b"]
	var draw_ladder: Dictionary = compute_ladder(draw_only, draw_strategies, DEFAULT_INITIAL_RATING, DEFAULT_K_FACTOR, 20)
	var draw_ratings: Dictionary = draw_ladder["ratings"]
	if absf(draw_ratings["a"] - draw_ratings["b"]) > 1.0:
		push_error("draft_elo_rating self-test: draw-only pair should have near-equal ratings")
		return false

	var self_play_biased: Array = [
		{
			"blue_strategy": "mirror",
			"red_strategy": "mirror",
			"blue_wins": 60,
			"red_wins": 40,
			"draws": 0,
		},
	]
	var mirror_strategies: Array[String] = ["mirror"]
	var mirror_ladder: Dictionary = compute_ladder(
		self_play_biased, mirror_strategies, DEFAULT_INITIAL_RATING, DEFAULT_K_FACTOR, 20
	)
	var mirror_rating: float = mirror_ladder["ratings"]["mirror"]
	if absf(mirror_rating - DEFAULT_INITIAL_RATING) > 1.0:
		push_error("draft_elo_rating self-test: self-play side bias should not move rating (got %.2f)" % mirror_rating)
		return false

	var mirror_stats: Dictionary = mirror_ladder["stats"]["mirror"]
	if int(mirror_stats["games"]) != 0:
		push_error("draft_elo_rating self-test: self-play should not contribute to stats games")
		return false

	return true
