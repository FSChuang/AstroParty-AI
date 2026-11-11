"""Focused checks for generated arena layout and the geometry oracle."""

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from generate_team_arena import InvalidArena, generate, opposite, point, position, validate


class GenerateArenaTests(unittest.TestCase):
    def test_size_presets_generate_symmetric_maps_with_matching_score_targets(self):
        for size, dimensions, score, duration, obstacles in (
            ("small", (960, 540), 3, None, 2),
            ("medium", (1280, 720), 5, None, 4),
            ("large", (1600, 900), 7, 90, 6),
        ):
            with self.subTest(size=size):
                arena = generate(42, size=size)
                self.assertEqual(point(arena["arenaSize"]), dimensions)
                self.assertEqual(arena["winningScore"], score)
                self.assertEqual(arena.get("matchDurationSeconds"), duration)
                self.assertEqual(len(arena["obstacles"]), obstacles)
                self.assertEqual(arena, generate(42, size=size))
                cores = [point(core) for core in arena["resourceSpawnPoints"]]
                self.assertEqual(set(cores), {opposite(core, dimensions) for core in cores})
                validate(arena, obstacles, 5, 18)

    def test_explicit_obstacle_count_overrides_the_preset(self):
        arena = generate(42, size="large", obstacles=8)
        self.assertEqual(len(arena["obstacles"]), 8)
        validate(arena, 8, 5, 18)

    def test_seed_repeats_layout_and_other_seeds_vary_it(self):
        arena = generate(12, 8, 5, "central")
        self.assertEqual(arena, generate(12, 8, 5, "central"))
        self.assertNotEqual(arena["obstacles"], generate(13, 8, 5, "central")["obstacles"])
        rocks = arena["obstacles"]
        for first, second in zip(rocks[::2], rocks[1::2]):
            self.assertEqual(opposite(point(first["position"])), point(second["position"]))
            self.assertEqual(first["radius"], second["radius"])
        validate(arena, 8, 5, 18)

    def test_blocked_resource_is_rejected(self):
        arena = generate(1)
        arena["resourceSpawnPoints"][0] = arena["obstacles"][0]["position"]
        with self.assertRaisesRegex(InvalidArena, "blocked resource"):
            validate(arena, 4, 5, 18)

    def test_wall_of_inflated_obstacles_disconnects_homes(self):
        arena = generate(1, 8)
        arena["obstacles"] = [
            {"position": position((730, 32 + 94 * i)), "radius": 30} for i in range(8)
        ]
        arena["resourceSpawnPoints"] = [
            position(xy) for xy in [(640, 360), (430, 250), (850, 470), (430, 470), (850, 250)]
        ]
        with self.assertRaisesRegex(InvalidArena, "disconnected"):
            validate(arena, 8, 5, 18)


if __name__ == "__main__":
    unittest.main()
