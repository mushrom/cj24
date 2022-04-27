#include <grend/ecs/sceneComponent.hpp>
#include <grend/gameMainDevWindow.hpp>
#include <grend/scancodes.hpp>
#include <grend/loadScene.hpp>
#include <iostream>

#include <grend/ecs/collision.hpp>
#include <grend/ecs/rigidBody.hpp>
#include <grend/ecs/shader.hpp>
#include <grend/interpolation.hpp>

#include "state.hpp"

using namespace grendx;
using namespace grendx::ecs;
using namespace grendx::interp;

/*
#include <grend/loadScene.hpp>
#include <grend/ecs/ecs.hpp>

using namespace grendx;
using namespace grendx::ecs;
*/

template <typename T>
T tryGet(nlohmann::json& value, const std::string& name, T defaultVal) {
	return value.contains(name)? value[name].get<T>() : defaultVal;
}

renderPostChain::ptr createPost(gameMain *game) {
	return renderPostChain::ptr(new renderPostChain(
		{loadPostShader(GR_PREFIX "shaders/baked/texpresent.frag",
						game->rend->globalShaderOptions)},
		game->settings.targetResX, game->settings.targetResY));
}

class gamething : public gameView {
	renderPostChain::ptr post;

	public:
		jamState state;
		enum Mode {
			Waiting,
			NewTurn,
		} mode = NewTurn;

		enum Objects {
			emptyTile,
			maxObjects,
		};

		sceneNode::ptr objects[maxObjects];
		sceneNode::ptr players[8];
		std::vector<sceneNode::ptr> mapNodes;
		std::set<int> nodeAnims;
		
		gamething(gameMain *game) {
			post = createPost(game);
			glm::vec3 dir = glm::normalize(glm::vec3(-0.71, -0.71, 0));
			cam->setDirection(dir);
			cam->setFar(1000);
			cam->setPosition({0, 10, 10});

			objects[emptyTile] = *loadSceneCompiled(DEMO_PREFIX "assets/obj/emptyTile.glb");

			for (unsigned i = 0; i < state.players.size(); i++) {
				printf("Loading %d\n", i);
				sceneLightPoint::ptr light = std::make_shared<sceneLightPoint>();
				sceneNode::ptr temp = std::make_shared<sceneNode>();
				sceneNode::ptr model = *loadSceneCompiled(DEMO_PREFIX "assets/obj/BoomBox.glb");
				model->setScale({100, 100, 100});

				setNode("light", temp, light);
				setNode("model", temp, model);

				players[i] = temp;
				std::string pname = std::string("player") + std::to_string(i); 
				setNode(pname, game->state->rootnode, temp);

				int k = i + 1;
				light->diffuse = {0.3 + 0.7*(k&4), 0.3 + 0.7*(k&2), 0.3 + 0.7*(k&1), 1};
				light->intensity = 100;
				light->casts_shadows = true;
				light->is_static = false;
			}
		};

		virtual void handleEvent(gameMain *game, const SDL_Event& ev) {
		}

		virtual void update(gameMain *game, float delta) {
			static smoothed<float> rads(0);
			static smoothed<float> speed(0, 128);
			static smoothed<glm::vec3> pos;

			static auto key_se = keyButton(SDL_SCANCODE_S);
			static auto key_sw = keyButton(SDL_SCANCODE_A);
			static auto key_ne = keyButton(SDL_SCANCODE_W);
			static auto key_nw = keyButton(SDL_SCANCODE_Q);

			for (auto it = nodeAnims.begin(); it != nodeAnims.end();) {
				int idx = *it;
				auto& node = mapNodes[idx];

				glm::vec3 pos    = node->getTransformTRS().position;
				glm::vec3 target = {pos[0], 0, pos[2]};

				if (abs(target.y - pos.y) < 0.0001) {
					printf("Removing anim for %d\n", idx);
					it = nodeAnims.erase(it);
				} else {
					it++;
				}

				glm::vec3 avg = average(target, pos, 16, delta);
				node->setPosition(avg);
			}

			/*
			static auto left  = keyButton(SDL_SCANCODE_LEFT);
			static auto right = keyButton(SDL_SCANCODE_RIGHT);
			static auto up    = keyButton(SDL_SCANCODE_UP);
			static auto down  = keyButton(SDL_SCANCODE_DOWN);
			static auto space = keyButton(SDL_SCANCODE_SPACE);
			static auto meh   = mouseButton(SDL_BUTTON_LEFT);
			*/

			rads.update(delta);
			speed.update(delta);
			pos.update(delta);

			if (state.playersAlive <= 1) {
				puts("Game over!");
				return;
			}

			int playerID = state.getPlayerNum();
			auto& player = state.getPlayer(playerID);
			cam->setPosition(pos);

			glm::vec3 ppos = {player.position.first, 1, -player.position.second};
			players[playerID]->setPosition(ppos);

			if (mode == NewTurn) {
				printf("==== player %d: %u moves left\n", playerID, player.movesLeft);
				printf("==== player %d: %u coins\n", playerID, player.coins);

				if (player.movesLeft == 0) {
					auto v = state.roll();
					unsigned num = state.sum(v);

					player.movesLeft = 20 * num;

					printf("==== rolled a %u, moves left: %u\n", num, player.movesLeft);

					if (num == 0) {
						printf("==== Player turn skipped!\n");
						state.nextPlayer();
						//getchar();
						//continue;
					}
				}

				for (auto& p : state.players) {
					std::set<Coord> endpoints = state.endpoints;

					for (auto& e : endpoints) {
						if (distance(p.position, e) < 12) {
							state.genTiles(e);
						}
					}
				}

				//print(state);

				unsigned gencount = 0;
				for (auto& v : state.getGeneratedtiles()) {
					char buf[64];
					snprintf(buf, sizeof(buf), "gen(%d,%d)", v.first, v.second);

					sceneNode::ptr foo = std::make_shared<sceneNode>();
					foo->setPosition({v.first, -(32.f + (32.f*gencount++)), -v.second});

					setNode("model", foo, objects[emptyTile]);
					setNode(buf, game->state->rootnode, foo);

					nodeAnims.insert(mapNodes.size());
					mapNodes.push_back(foo);
					//printf("Generated (%d, %d)\n", v.first, v.second);
				}

				//pos = {player.position.first - 10, 10, player.position.second - 10};
				glm::vec3 temp = {player.position.first, 0, -player.position.second};

				pos = temp - cam->direction()*20.f;
				mode = Waiting;

			} else if (mode == Waiting) {
				if (keyJustPressed(key_se)) {
					state.makeMove(jamState::SouthEast);
					mode = NewTurn;

				} else if (keyJustPressed(key_sw)) {
					state.makeMove(jamState::SouthWest);
					mode = NewTurn;

				} else if (keyJustPressed(key_ne)) {
					state.makeMove(jamState::NorthEast);
					mode = NewTurn;

				} else if (keyJustPressed(key_nw)) {
					state.makeMove(jamState::NorthWest);
					mode = NewTurn;
				}

				if (player.movesLeft == 0) {
					state.nextPlayer();
				}
			}

			/*
			game->phys->stepSimulation(delta);
			game->phys->filterCollisions();;
			*/

			game->entities->update(delta);

			/*
			auto temp = game->entities->search<rigidBody>();
			for (entity *ent : temp) {
				if (game->entities->condemned.count(ent)) {
					std::cout << "[deleted] " << std::endl;
					continue;
				}

				if (!ent->active)
					continue;

				glm::vec3 pos = ent->node->getTransformTRS().position;
				glm::vec3 dir = cam->direction();
				static smoothed<float> x;
				//x = 4*min(2.f, keyHoldTime(meh));
				x = 8.f * keyIsPressed(meh);
				x.update(delta);
				cam->setPosition((glm::vec3)pos - dir*(10.f + (*x)*(*x)));

				rigidBody *body = ent->get<rigidBody>();
				glm::vec3 vel;

				if (keyIsPressed(left))  rads += 0.1; 
				if (keyIsPressed(right)) rads -= 0.1; 

				cam->setDirection(glm::vec3 {
					cos(rads),
					-0.71,
					sin(rads)
				});

				speed = glm::length(body->phys->getVelocity());
				//speed.update(delta);

				if (keyIsPressed(up)) {
					glm::vec3 dir = glm::cross(cam->right(), glm::vec3(0, 1, 0));
					//vel = dir * (speed + keyHoldTime(up)*0.5f);
					//vel = dir * ((1 + keyHoldTime(up))*10.f + keyIsPressed(space)*100.f);
					glm::vec3 foo = glm::normalize(body->phys->getVelocity());
					float asdf = 2.f - (glm::dot(foo, dir) + 1.f);
					vel = dir * 10.f*(1.f + 10*asdf);
					body->phys->setAcceleration(vel);
				}

				if (keyIsPressed(down)) {
					glm::vec3 dir = glm::cross(cam->right(), glm::vec3(0, 1, 0));
					vel = dir * -10.f;
					body->phys->setVelocity(vel);
				}

				cam->setFovx(80 + 4 * min(*speed, 40.f));
				break;
			}
			*/
		};

		virtual void render(gameMain *game, renderFramebuffer::ptr fb) {
			auto que = buildDrawableQueue(game, cam);
			que.add(game->rend->getLightingFlags(), game->state->rootnode);
			drawMultiQueue(game, que, game->rend->framebuffer, cam);
			game->rend->defaultSkybox.draw(cam, game->rend->framebuffer);
			post->draw(game->rend->framebuffer);
		};
};

int main(void) {
	gameMain *game = new gameMainDevWindow({ .UIScale = 2.0 });


	game->rend->defaultSkybox = skybox("share/proj/assets/tex/cubes/HeroesSquare/", ".jpg");
	/*
	if (auto p = loadSceneCompiled(DEMO_PREFIX "assets/obj/planething.glb")) {
		game->state->rootnode = *p;
	}
	*/

	auto view = std::make_shared<gamething>(game);
	game->setView(view);

	sceneLightDirectional::ptr p = std::make_shared<sceneLightDirectional>();
	p->setRotation(glm::vec3 {0, 0, -M_PI/2});

	setNode("asdfasdf", game->state->rootnode, p);
	/*
	static std::vector<physicsObject::ptr> mapPhysics;
	if (auto p = loadMapCompiled("save.map")) {
		game->state->rootnode = *p;
		game->phys->addStaticModels(nullptr, *p, TRS(), mapPhysics);
	}
	*/

	game->run();

	std::cout << "It's alive!" << std::endl;
	return 0;
}
