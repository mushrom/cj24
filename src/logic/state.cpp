#include "state.hpp"

void print(jamState& state) {
	int curline =  0;
	int curchar = -60;

	for (auto& t : state.tiles) {
		const Coord& p = t.first;

		while (curline < p.first) {
			putchar('\n');
			curline++;
			curchar = -60;
		}

		while (curchar < p.second) {
			putchar(' ');
			curchar++;
		}

		for (unsigned i = 0; i < state.players.size(); i++) {
			auto& p = state.players[i];

			if (Coord {curline, curchar} == p.position) {
				putchar('0' + i);
				curchar++;
				goto done;
			}
		}

		switch (t.second.type) {
			//case tileState::Drawbridge: putchar('='); break;
			//case tileState::Ambush:     putchar('A'); break;
			default: putchar('#'); break;
		}

		curchar++;

done: ;
	}

	putchar('\n');
}

#ifdef AN_UNDEFINED_THING
auto main() -> int try {
	jamState state;

	srand(time(NULL));

	for (int i = 0;; i++) {
		int playerID = state.getPlayerNum();
		auto& player = state.getPlayer(playerID);

		//printf("\x1b\x5b\x48\x1b\x5b\x32\x4a\x1b\x5b\x33\x4a");
		printf("==== %d: %lu tiles\n", i, state.tiles.size());
		printf("==== player %d: %u moves left\n", playerID, player.movesLeft);

		if (player.movesLeft == 0) {
			auto v = state.roll();
			unsigned num = state.sum(v);

			player.movesLeft = 2 * num;

			printf("==== rolled a %u, moves left: %u\n", num, player.movesLeft);

			if (num == 0) {
				printf("==== Player turn skipped!\n");
				state.nextPlayer();
				//getchar();
				continue;
			}
		}

		print(state);

		//state.genTiles();
		
		for (auto& v : state.getGeneratedtiles()) {
			//printf("Generated (%d, %d)\n", v.first, v.second);
		}

		//getchar();
		//sleep(1);
		for (bool hasChar = false; !hasChar;) {
			char c = getchar();

			switch (c) {
				case 'q':
					hasChar = true;
					state.makeMove(jamState::NorthWest);
					break;

				case 'e':
					hasChar = true;
					state.makeMove(jamState::NorthEast);
					break;

				case 'z':
					hasChar = true;
					state.makeMove(jamState::SouthWest);
					break;

				case 'c':
					hasChar = true;
					state.makeMove(jamState::SouthEast);
					break;

				default:
					break;
			}
		}

		if (player.movesLeft == 0) {
			state.nextPlayer();
		}

		for (auto& p : state.players) {
			for (auto& e : state.endpoints) {
				if (distance(p.position, e) < 7) {
					state.genTiles();
				}
			}
		}
	}

	return 0;
} catch (const std::exception& ex) {}
#endif
