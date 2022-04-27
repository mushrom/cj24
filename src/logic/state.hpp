#pragma once

#include <exception>
#include <utility>
#include <tuple>
#include <set>
#include <map>
#include <vector>

#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <unistd.h>

typedef std::pair<int, int> Coord;

static inline
Coord operator +(const Coord& a, const Coord& b) {
	return {a.first + b.first, a.second + b.second};
}

static inline
Coord& operator +=(Coord& a, const Coord& b) {
	a.first  += b.first;
	a.second += b.second;

	return a;
}

static inline
Coord operator*(const Coord& a, int x) {
	return {a.first * x, a.second * x};
}

struct tileState {
	enum Type {
		Empty,
		Drawbridge,
		Ambush,
		AttackBonus,
		SpeedBonus,
		CoinBonus,
		CoinTrap,
		TypeEnd,
	} type = Empty;

	bool traversed = false;
	bool activated = false;

	unsigned coinsRequired = 0;
};

struct player {
	Coord position = {0, 0};
	bool alive     = true;

	unsigned coins     = 2;
	unsigned gold      = 0;
	unsigned movesLeft = 0;

	float attackBuf = 1.0;
	float speedBuf  = 1.0;
};

struct jamState {
	using Tile = std::pair<Coord, tileState>;

	static constexpr Coord NorthEast = {-1,  1};
	static constexpr Coord NorthWest = {-1, -1};
	static constexpr Coord SouthEast = { 1,  1};
	static constexpr Coord SouthWest = { 1, -1};

	jamState(unsigned numPlayers = 2) {
		Tile start = {{0, 0}, {tileState::Empty}};

		tiles.insert(start);
		endpoints.insert(start.first);
		generated.insert(start.first);

		players.resize(numPlayers);
		playersAlive = numPlayers;

		genTiles(start.first);
	}

	std::map<Coord, tileState> tiles;
	std::vector<player> players;
	std::set<Coord> endpoints;
	std::set<Coord> generated;
	unsigned currentPlayer = 0;
	unsigned playersAlive = 0;

	std::set<Coord> getGeneratedtiles(void) {
		std::set<Coord> ret(generated.begin(), generated.end());

		generated.clear();

		return ret;
	}

	void genTiles(const Coord& end) {
		std::vector<Tile> ret;
		//std::set<Coord> newEndpoints;

		//for (auto& end : endpoints) {
			std::set<Coord> dirs;

			//unsigned count = ((rand() % 10) > 5)? 2 : 1;
			unsigned count = 2;
			for (unsigned i = 0; i < count; i++) {
				Coord dir = (rand() & 1)? SouthWest : SouthEast;
				dirs.insert(dir);
			}

			for (auto& dir : dirs) {
				unsigned length = 2*(1 + rand() % 3);

				for (unsigned i = 1; i <= length; i++) {
					Coord next = end + dir*i;

					if (surrounding(next) <= 2 && !tiles.contains(next)) {
						tileState t;

						if (rand() % 4 > 0) {
							t.type          = (tileState::Type)(rand() % tileState::TypeEnd);
							t.coinsRequired = rand()%6 + 1;

						} else {
							t.type = tileState::Empty;
						}

						tiles.insert({next, t});
						generated.insert(next);

					} else {
						goto done;
						// cant be an endpoint
						continue;
						break;
					}
				}

				if (surrounding(end + dir*length) == 1) {
					//newEndpoints.insert(end + dir*length);
					endpoints.insert(end + dir*length);
				}
done: ;
			}
		//}

			endpoints.erase(end);
		//endpoints = newEndpoints;
	}

	unsigned getPlayerNum() {
		return currentPlayer;
	}

	player& getPlayer(unsigned n) {
		return players[n];
	}

	unsigned nextPlayer(void) {
		for (unsigned i = 0; i < players.size(); i++) {
			currentPlayer = (currentPlayer + 1) % players.size();
			player& p = getPlayer(currentPlayer);

			if (p.alive)
				break;
		}

		return currentPlayer;
	}
	
	std::vector<unsigned> roll(void) {
		auto& p = getPlayer(getPlayerNum());

		std::vector<unsigned> ret;
		ret.resize(p.coins);

		for (unsigned i = 0; i < p.coins; i++) {
			ret[i] = rand() & 1;
		}

		return ret;
	}

	unsigned sum(std::vector<unsigned>& coins) {
		unsigned ret = 0;

		for (auto& i : coins) {
			ret += i;
		}

		return ret;
	}

	void makeMove(const Coord& direction) {
		auto& p = getPlayer(getPlayerNum());

		if (!tiles.contains(p.position)) {
			printf("Player is not on a tile!");
		}

		if (!p.alive || p.movesLeft == 0) {
			return;
		}

		if (validMove(direction)) {
			p.position += direction;
			tileState& t = tiles[p.position];

			switch (t.type) {
				/*
				case tileState::CoinBonus:
					p.coins++;
					break;

				case tileState::CoinTrap:
					p.coins--;

					if (p.coins <= 0) {
						p.alive = false;
					}

					break;
					*/

				default:
					break;
			}

			p.movesLeft--;
		}

		/*
		if (p.movesLeft == 0) {
			unsigned roll = 0;

			for (unsigned i = 0; i < p.coins; i++) {
				roll += rand() & 1;
			}
		}
		*/
	}

	bool validMove(const Coord& direction) {
		auto& p = getPlayer(getPlayerNum());
		return tiles.contains(p.position + direction);
	}

	std::set<Coord> validMoves() {
		std::set<Coord> ret;

		for (int x = -1; x <= 1; x += 2) {
			for (int y = -1; y <= 1; y += 2) {
				Coord dir = {y, x};
				if (validMove(dir)) {
					ret.insert(dir);
				}
			}
		}

		return ret;
	}

	unsigned surrounding(const Coord& position) {
		unsigned count = 0;

		for (int x = -1; x <= 1; x += 2) {
			for (int y = -1; y <= 1; y += 2) {
				Coord pos = position + Coord {y, x};
				count += tiles.contains(pos);
			}
		}

		return count;
	}

	/*
	unsigned perpendicular(const Coord& position, const Coord& dir) {
		unsigned count = 0;
		Coord end = position + dir;

		for (int x = -1; x <= 1; x += 2) {
			for (int y = -1; y <= 1; y += 2) {
				Coord pos = position + Coord {y, x};

				if (pos == end || pos == position)
					continue;

				count += tiles.contains(pos);
			}
		}

		return count;
	}
	*/
};

void print(jamState& state);

static inline
int distance(const Coord& a, const Coord& b) {
	return abs(a.first - b.first) + abs(a.second - b.second);
}

