#pragma once

#include <exception>
#include <utility>
#include <tuple>
#include <set>
#include <map>
#include <vector>
#include <deque>
#include <optional>

#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <unistd.h>

#define MAX_COINS 6

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

static inline
int distance(const Coord& a, const Coord& b) {
	return abs(a.first - b.first) + abs(a.second - b.second);
}

struct tileState {
	enum Type {
		Empty,
		/*
		Drawbridge,
		Ambush,
		AttackBonus,
		SpeedBonus,
		*/
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
	float rotation = 0;
	bool alive     = true;

	unsigned coins     = 4;
	unsigned gold      = 0;
	unsigned movesLeft = 0;

	float attackBuf = 1.0;
	float speedBuf  = 1.0;

	std::set<Coord> traversed;
};

// TODO: why not just "jam" namespace
struct jamEvent {
	enum Type {
		Moved,
		Bonus,
		Trap,
		CoinGain,
		CoinLoss,
		NotEnoughCoin,
		CoinNotGained,
		LossAvoided,
		MaxCoinsReached,
	} type;

	unsigned rollThresh = 0;
	unsigned rollVal = 0;
	bool rollSuccess = false;

	std::vector<bool> flips;
};

struct jamState {
	using Tile = std::pair<Coord, tileState>;

	static constexpr Coord NorthEast = {-1,  1};
	static constexpr Coord NorthWest = {-1, -1};
	static constexpr Coord SouthEast = { 1,  1};
	static constexpr Coord SouthWest = { 1, -1};

	jamState(unsigned numPlayers = 2) {
		reset(numPlayers);
	}

	void reset(unsigned numPlayers = 2) {
		tiles.clear();
		endpoints.clear();
		generated.clear();
		players.clear();

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
	std::deque<jamEvent> events;
	std::set<Coord> endpoints;
	std::set<Coord> generated;
	unsigned currentPlayer = 0;
	unsigned playersAlive = 0;

	std::set<Coord> getGeneratedtiles(void) {
		/*
		std::set<Coord> ret(generated.begin(), generated.end());

		generated.clear();
		*/

		return std::move(generated);
		//return ret;
	}

	/*
	std::vector<jamEvent> getEvents() {
		return std::move(events);
	}
	*/

	std::optional<jamEvent> getEvent() {
		if (events.size() > 0) {
			jamEvent ret = events.front();
			events.pop_front();
			return ret;

		} else {
			return {};
		}
	}

	void addEvent(const jamEvent& ev) {
		events.push_back(ev);
	}

	void genTiles(const Coord& end) {
		std::vector<Tile> ret;
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

					if (rand() % 8 > 5) {
						auto choice = (rand()%4 > 1)
							? tileState::CoinBonus
							: tileState::CoinTrap;

						t.type = choice;
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

		endpoints.erase(end);
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

	void ripPlayer(void) {
		auto& p = getPlayer(getPlayerNum());

		if (p.alive) {
			p.alive = false;
			playersAlive--;
		}
	}
	
	std::vector<bool> roll(void) {
		auto& p = getPlayer(getPlayerNum());

		std::vector<bool> ret;
		ret.resize(p.coins);

		for (unsigned i = 0; i < p.coins; i++) {
			ret[i] = rand() & 1;
		}

		return ret;
	}

	unsigned sum(std::vector<bool>& coins) {
		unsigned ret = 0;

		for (unsigned i = 0; i < coins.size(); i++) {
			ret += coins[i];
		}

		return ret;
	}

	void makeMove(const Coord& direction) {
		auto& p = getPlayer(getPlayerNum());

		if (!tiles.contains(p.position)) {
			printf("Player is not on a tile!");
		}

		if (numValidMoves() == 0) {
			puts("Player ran out of valid moves, ded!");
			ripPlayer();
		}

		if (!p.alive || p.movesLeft == 0) {
			return;
		}

		if (validMove(direction)) {
			p.traversed.insert(p.position);

			p.position += direction;
			tileState& t = tiles[p.position];

			addEvent({ .type = jamEvent::Moved, });

			switch (t.type) {
				case tileState::CoinBonus: {
					if (p.coins >= t.coinsRequired && p.coins < MAX_COINS) {
						jamEvent ev = {
							.type = jamEvent::Bonus,
							.rollThresh = t.coinsRequired,
							.flips = roll(),
						};

						ev.rollVal = sum(ev.flips);
						ev.rollSuccess = ev.rollVal >= ev.rollThresh;
						addEvent(ev);

						if (ev.rollSuccess && p.coins < MAX_COINS) {
							addEvent({ .type = jamEvent::CoinGain });
							p.coins++;

						} else {
							addEvent({ .type = jamEvent::CoinNotGained });
						}

					} else if (p.coins < t.coinsRequired) {
						addEvent({ .type = jamEvent::NotEnoughCoin });

					} else {
						addEvent({ .type = jamEvent::MaxCoinsReached });
					}

					break;
				}

				case tileState::CoinTrap: {
					bool success = false;
					if (p.coins >= t.coinsRequired) {
						jamEvent ev = {
							.type = jamEvent::Bonus,
							.rollThresh = t.coinsRequired,
							.flips = roll(),
						};

						ev.rollVal = sum(ev.flips);
						success = ev.rollSuccess = ev.rollVal >= ev.rollThresh;
						addEvent(ev);

					} else {
						//addEvent({ .type = jamEvent::NotEnoughCoin });
					}

					if (!success) {
						p.coins--;
						addEvent({ .type = jamEvent::CoinLoss });

						if (p.coins <= 0) {
							ripPlayer();
						}

					} else {
						addEvent({ .type = jamEvent::LossAvoided });
					}
				}
				break;

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

		Coord newpos = p.position + direction;
		return !p.traversed.contains(newpos) && tiles.contains(newpos);
	}

	unsigned numValidMoves() {
		unsigned ret = 0;

		for (int x = -1; x <= 1; x += 2) {
			for (int y = -1; y <= 1; y += 2) {
				Coord dir = {y, x};

				ret += validMove(dir);
			}
		}

		return ret;
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

	void tryExpanding(float thresh = 12) {
		std::set<Coord> endp = endpoints;

		for (auto& p : players) {
			for (auto& e : endp) {
				if (distance(p.position, e) < thresh) {
					genTiles(e);
				}
			}
		}
	}
};

void print(jamState& state);

