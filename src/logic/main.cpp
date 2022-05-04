#include <grend/ecs/sceneComponent.hpp>
#include <grend/gameMainDevWindow.hpp>
#include <grend/scancodes.hpp>
#include <grend/loadScene.hpp>
#include <iostream>

#include <grend/ecs/collision.hpp>
#include <grend/ecs/rigidBody.hpp>
#include <grend/ecs/shader.hpp>
#include <grend/interpolation.hpp>

#include <nuklear/nuklear.h>
#include <nuklear/canvas.h>

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
#if 1
void drawPlayerHealthbar(gameMain *game,
                         struct nk_context *nk_ctx,
                         struct nk_user_font *font,
                         const std::string& txt,
                         glm::ivec2 coord)
{
	int wx = game->rend->screen_x;
	int wy = game->rend->screen_y;

	glm::ivec2 dim = {20*txt.size(), 48};

	std::string xstr = std::to_string(coord.x);
	std::string ystr = std::to_string(coord.y);

	std::string fullstr = txt+":"+xstr+":"+ystr;

	struct nk_canvas canvas;
	if (nk_canvas_begin(nk_ctx, &canvas, fullstr.c_str(), 0,
	                    coord.x, coord.y, dim.x, dim.y,
	                    nk_rgba(32, 32, 32, 0)))
	{
		float k = 252;
		//struct nk_rect rect  = nk_rect(93, 50, k, 12);
		struct nk_rect rect  = nk_rect(coord.x, coord.y, dim.x, dim.y);

		/*
		nk_fill_rect(canvas.painter, rect, 0, nk_rgb(192, 64, 32));
		*/

		nk_draw_text(canvas.painter, rect, txt.c_str(), txt.size(),
				font,
				nk_rgba(0,0,0,255), nk_rgba(192, 192, 192, 255)
				);
	}
	nk_canvas_end(nk_ctx, &canvas);

	/*
	if (nk_begin(nk_ctx, "Inventory info", nk_rect(90, 72, 192, 32), 0)) {
		nk_layout_row_dynamic(nk_ctx, 14, 1);
		nk_label(nk_ctx, "[Tab/Start] Open inventory", NK_TEXT_LEFT);
	}
	nk_end(nk_ctx);

	if (nk_begin(nk_ctx, "FPS", nk_rect(wx - 144, 48, 112, 32), 0)) {
		double fps = manager->engine->frame_timer.average();
		std::string fpsstr = std::to_string(fps) + "fps";

		nk_layout_row_dynamic(nk_ctx, 14, 1);
		nk_label(nk_ctx, fpsstr.c_str(), NK_TEXT_LEFT);
	}
	nk_end(nk_ctx);
	*/
}
#endif


class traversedParticles : public entity {
	public:
		traversedParticles(ecs::regArgs args);
		virtual ~traversedParticles();
		void update(player& pl);

		sceneBillboardParticles::ptr parts;
		glm::vec3 positions[128];
};

traversedParticles::traversedParticles(regArgs args)
	: entity(doRegister(this, args))
{
	static sceneNode::ptr model = nullptr;

	attach<PBRShader>();

	parts = std::make_shared<sceneBillboardParticles>(128);
	parts->activeInstances = 0;
	parts->radius = 10000.f;
	parts->update();

	if (!model) {
		auto data = loadSceneCompiled(DEMO_PREFIX "assets/obj/emissive-plane.glb");
		if (data)
			model = *data;
	}

	setNode("model", parts, model);
	setNode("parts", node, parts);
}

traversedParticles::~traversedParticles() {
	parts->activeInstances = 0;
	parts->update();
}

void traversedParticles::update(player& pl) {
	unsigned i = 0;
	unsigned lim = min(128u, pl.traversed.size());
	auto it = pl.traversed.rbegin();

	for (; i < lim; it++, i++) {
		parts->positions[i] = glm::vec4(it->first, 1, -it->second, 0.5);
	}

	parts->activeInstances = lim;
	parts->update();

	/*
	for (unsigned i = 0; i < 128; i++) {
		positions[i] += velocities[i]*delta;
		positions[i].y = max(0.2f, positions[i].y);
		velocities[i].y -= delta*30;
		time += delta;

		if (positions[i].y < 0.3) {
			velocities[i]   *= 0.8;
		}

		parts->positions[i] = glm::vec4(positions[i], 0.5);
	}

	parts->update();
	*/
}

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
			Flip,
		} mode = NewTurn;

		enum Objects {
			emptyTile,
			coinBonusTile,
			coinTrapTile,
			maxObjects,
		};

		sceneNode::ptr objects[maxObjects];
		sceneNode::ptr players[8];
		sceneNode::ptr numbers[9];
		traversedParticles *particles[8];

		struct nk_context *nk_ctx;
		struct nk_font *roboto;
		struct nk_font *robotoSmall;

		std::vector<sceneNode::ptr> mapNodes;
		std::set<int> nodeAnims;

		std::vector<float> coinTargets;
		std::vector<float> coinCur;

		std::string displayedMsg = "Current coins: 4";
		std::string traversedMsg = "Tiles traversed: 0";
		std::vector<std::string> notifications;

		sceneNode::ptr coinNodes = std::make_shared<sceneNode>();
		sceneNode::ptr coinModel = *loadSceneCompiled(DEMO_PREFIX "assets/obj/coin.glb");
		sceneNode::ptr playerModel = *loadSceneCompiled(DEMO_PREFIX "assets/obj/eyeball.glb");
		sceneNode::ptr mapRoot;

		channelBuffers_ptr goodsfx = openAudio(DEMO_PREFIX "assets/sfx/good.ogg");
		channelBuffers_ptr badsfx  = openAudio(DEMO_PREFIX "assets/sfx/bad.ogg");

		void reset(gameMain *game) {
			state.reset(1);
			mapNodes.clear();
			nodeAnims.clear();
			coinTargets.clear();
			coinCur.clear();

			mapRoot = std::make_shared<sceneNode>();
			setNode("map", game->state->rootnode, mapRoot);

			// TODO: FIXME: doesn't find entity here, because the particle systems
			//              don't get an entity attached?
			for (entity *ent : game->entities->search<component>()) {
				printf("removing %p\n", ent);
				game->entities->remove(ent);
			}

			for (unsigned i = 0; i < state.players.size(); i++) {
				printf("Loading %d\n", i);
				sceneLightPoint::ptr light = std::make_shared<sceneLightPoint>();
				sceneNode::ptr temp = std::make_shared<sceneNode>();

				//sceneNode::ptr model = *loadSceneCompiled(DEMO_PREFIX "assets/obj/BoomBox.glb");
				//model->setScale({100, 100, 100});
				sceneNode::ptr model = playerModel;
				//sceneNode::ptr model = *loadSceneCompiled(DEMO_PREFIX "assets/obj/coin.glb");

				setNode("light", temp, light);
				setNode("model", temp, model);

				players[i] = temp;
				std::string pname = std::string("player") + std::to_string(i); 
				setNode(pname, mapRoot, temp);

				int k = i + 1;
				light->diffuse = {0.3 + 0.7*(k&4), 0.3 + 0.7*(k&2), 0.3 + 0.7*(k&1), 1};
				light->intensity = 100;
				light->casts_shadows = true;
				light->is_static = false;

				particles[i] = game->entities->construct<traversedParticles>(nullptr);
				game->entities->add(particles[i]);
			}

			coinNodes = std::make_shared<sceneNode>();
			setNode("coins", mapRoot, coinNodes);
		}
		
		gamething(gameMain *game) {
			post = createPost(game);
			glm::vec3 dir = glm::normalize(glm::vec3(-0.71, -0.71, 0));
			cam->setDirection(dir);
			cam->setFar(1000);
			cam->setPosition({0, 10, 10});

			objects[emptyTile] = *loadSceneCompiled(DEMO_PREFIX "assets/obj/emptyTile.glb");
			objects[coinBonusTile] = *loadSceneCompiled(DEMO_PREFIX "assets/obj/coinBonusTile.glb");
			objects[coinTrapTile] = *loadSceneCompiled(DEMO_PREFIX "assets/obj/coinLossTile.glb");

			for (unsigned i = 0; i < 9; i++) {
				numbers[i] = *loadSceneCompiled(DEMO_PREFIX "assets/obj/numbers/" + std::to_string(i) + ".gltf");
			}

			reset(game);

			nk_ctx = nk_sdl_init(game->ctx.window);
			if (!nk_ctx) {
				throw std::logic_error("Couldn't initialize nk_ctx!");
			}
			{
				struct nk_font_atlas *atlas;
				nk_sdl_font_stash_begin(&atlas);
				roboto = nk_font_atlas_add_from_file(atlas, GR_PREFIX "assets/fonts/Roboto-Regular.ttf", 48, 0);
				robotoSmall = nk_font_atlas_add_from_file(atlas, GR_PREFIX "assets/fonts/Roboto-Regular.ttf", 36, 0);
				nk_sdl_font_stash_end();

				if (roboto) {
					nk_style_set_font(nk_ctx, &roboto->handle);
				}
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
					//printf("Removing anim for %d\n", idx);
					it = nodeAnims.erase(it);
				} else {
					it++;
				}

				glm::vec3 avg = average(target, pos, 16, delta);
				node->setPosition(avg);
			}

			rads.update(delta);
			speed.update(delta);
			pos.update(delta);

			if (state.playersAlive == 0) {
				puts("Game over!");
				reset(game);

				return;
			}

			int playerID = state.getPlayerNum();

			while (true) {
				int playerID = state.getPlayerNum();
				auto& player = state.getPlayer(playerID);

				if (!player.alive) state.nextPlayer();
				else               break;
			}

			auto& player = state.getPlayer(playerID);
			cam->setPosition(pos);

			glm::vec3 ppos = {player.position.first, 0.5, -player.position.second};
			players[playerID]->setPosition(ppos);
			players[playerID]->setRotation(glm::vec3 {0, player.rotation, 0});

			traversedMsg = "Tiles traversed: " + std::to_string(player.traversed.size());

			if (mode == NewTurn) {
				printf("==== player %d: %u moves left\n", playerID, player.movesLeft);
				printf("==== player %d: %u coins\n", playerID, player.coins);

				if (player.movesLeft == 0) {
					auto v = state.roll();
					unsigned num = state.sum(v) + 1;

					player.movesLeft = 5 * num;

					printf("==== rolled a %u, moves left: %u\n", num, player.movesLeft);

					if (num == 0) {
						printf("==== Player turn skipped!\n");
						state.nextPlayer();
					}
				}

				state.tryExpanding();
				//print(state);
				
				unsigned gencount = 0;
				for (auto& v : state.getGeneratedtiles()) {
					char buf[64];
					snprintf(buf, sizeof(buf), "gen(%d,%d)", v.first, v.second);

					sceneNode::ptr foo = std::make_shared<sceneNode>();
					foo->setPosition({v.first, -(32.f + (32.f*gencount++)), -v.second});

					sceneNode::ptr model;
					auto& t = state.tiles[v];
					switch (t.type) {
						case tileState::CoinBonus:
							setNode("model", foo, objects[coinBonusTile]);
							break;

						case tileState::CoinTrap:
							setNode("model", foo, objects[coinTrapTile]);
							break;

						default:
							setNode("model", foo, objects[emptyTile]);
							break;
					}

					if (t.coinsRequired) {
						sceneNode::ptr temp = std::make_shared<sceneNode>();
						temp->setPosition({ 0.25, 0.25, 0 });
						temp->setScale(glm::vec3 {0.25});

						setNode("number", temp, numbers[t.coinsRequired]);
						setNode("indicator", foo, temp);
					}

					setNode(buf, mapRoot, foo);

					nodeAnims.insert(mapNodes.size());
					mapNodes.push_back(foo);
					//printf("Generated (%d, %d)\n", v.first, v.second);
				}

				glm::vec3 temp = {player.position.first, 0, -player.position.second};

				pos = temp - cam->direction()*20.f;
				mode = Waiting;

			} else if (mode == Flip) {
				float avgdiff = 0;

				for (unsigned i = 0; i < coinTargets.size(); i++) {
					std::string name = "node" + std::to_string(i);

					sceneNode::ptr node = coinNodes->getNode(name);
					if (!node) continue;

					coinCur[i] = average(coinTargets[i], coinCur[i], 12.f, delta);
					node->setRotation(glm::vec3 { 0, 0, coinCur[i] });

					avgdiff += fabs(coinTargets[i] - coinCur[i]);
				}

				if (avgdiff/coinTargets.size() < 0.05) {
					coinNodes->nodes.clear();
					mode = Waiting;
				}

			} else if (mode == Waiting) {
				auto newturn = [&] (const Coord& dir) {
					state.makeMove(dir);
					mode = NewTurn;
					particles[playerID]->update(player);
				};

				if (keyJustPressed(key_se)) {
					newturn(jamState::SouthEast);
					player.rotation = M_PI/2;

				} else if (keyJustPressed(key_sw)) {
					newturn(jamState::SouthWest);
					player.rotation = 0;

				} else if (keyJustPressed(key_ne)) {
					newturn(jamState::NorthEast);
					player.rotation = M_PI;

				} else if (keyJustPressed(key_nw)) {
					newturn(jamState::NorthWest);
					player.rotation = 3*M_PI/2;
				}

				if (auto evopt = state.getEvent()) {
					auto ev = *evopt;
					printf("have event: %d, success: %d\n", ev.type, ev.rollSuccess);

					printf("            ");
					for (unsigned i = 0; i < ev.flips.size(); i++) {
						printf("%d", (int)ev.flips[i]);
					}
					printf("\n");

					if (ev.flips.size() > 0) {
						mode = Flip;
						coinTargets.resize(ev.flips.size());
						coinCur.resize(ev.flips.size());
						coinNodes->nodes.clear();

						for (int i = 0; i < ev.flips.size(); i++) {
							coinTargets[i] = (ev.flips[i]? 1333*M_PI : 1110*M_PI) - M_PI/4;
							coinCur[i]     = 0.f;

							sceneNode::ptr temp = std::make_shared<sceneNode>();

							glm::vec3 npos = { -2, 2.f, -2 + 2.f * -i };
							temp->setPosition(ppos + npos);

							setNode("model", temp, coinModel);
							setNode("node" + std::to_string(i), coinNodes, temp);
						}
					}

					if (ev.type == jamEvent::NotEnoughCoin) {
						puts("bad sfx (not enough)");
						auto ch = std::make_shared<stereoAudioChannel>(badsfx);
						game->audio->add(ch);

						displayedMsg = "Current coins: " + std::to_string(player.coins);
						notifications.push_back("Not enough coins to roll");
					}

					if (ev.type == jamEvent::CoinLoss) {
						puts("bad sfx (coin loss)");
						auto ch = std::make_shared<stereoAudioChannel>(badsfx);
						game->audio->add(ch);

						displayedMsg = "Current coins: " + std::to_string(player.coins);
						notifications.push_back("Roll failed, lost a coin...");
					}

					if (ev.type == jamEvent::CoinGain) {
						puts("good sfx (coin gain)");
						auto ch = std::make_shared<stereoAudioChannel>(goodsfx);
						game->audio->add(ch);

						displayedMsg = "Current coins: " + std::to_string(player.coins);
						notifications.push_back("Roll successful, gained a coin!");
					}

					if (ev.type == jamEvent::CoinNotGained) {
						puts("bad sfx (coin not gained)");
						auto ch = std::make_shared<stereoAudioChannel>(badsfx);
						game->audio->add(ch);

						notifications.push_back("Roll failed, no coins gained");
					}

					if (ev.type == jamEvent::LossAvoided) {
						puts("good sfx (coin not lost)");
						auto ch = std::make_shared<stereoAudioChannel>(goodsfx);
						game->audio->add(ch);

						notifications.push_back("Roll successful, no coins lost");
					}

					if (ev.type == jamEvent::MaxCoinsReached) {
						puts("good sfx (coin not lost)");
						//auto ch = std::make_shared<stereoAudioChannel>(goodsfx);
						//game->audio->add(ch);

						notifications.push_back("Carrying maximum number of coins already!");
					}

				} else if (player.movesLeft == 0) {
					state.nextPlayer();
				}
			}

			/*
			game->phys->stepSimulation(delta);
			game->phys->filterCollisions();;
			*/

			game->entities->clearFreedEntities();
			game->entities->update(delta);
		};

		virtual void render(gameMain *game, renderFramebuffer::ptr fb) {
			auto que = buildDrawableQueue(game, cam);
			que.add(game->rend->getLightingFlags(), game->state->rootnode);
			drawMultiQueue(game, que, game->rend->framebuffer, cam);
			//game->rend->defaultSkybox.draw(cam, game->rend->framebuffer);
			post->draw(game->rend->framebuffer);



			nk_input_begin(nk_ctx);
			drawPlayerHealthbar(game, nk_ctx, &roboto->handle, displayedMsg, {48, 48});
			drawPlayerHealthbar(game, nk_ctx, &roboto->handle, traversedMsg, {48, 48*2});

			auto it = notifications.rbegin();
			unsigned i = 0;
			for (; it != notifications.rend() && i < 1; it++, i++) {
				drawPlayerHealthbar(game, nk_ctx, &robotoSmall->handle, *it, {48, 48*3 + 36*i});
			}

			nk_sdl_render(NK_ANTI_ALIASING_ON, 512*1024, 128*1024);
			nk_input_end(nk_ctx);
		};
};

int main(void) {
	renderSettings foo = (renderSettings){
		.scaleX = 1.0,
		.scaleY = 1.0,
		.targetResX = 1280,
		.targetResY = 720,
		.fullscreen = false,
		.windowResX = 1280,
		.windowResY = 720,
		.UIScale = 1.0,
	};

	srand(time(NULL));
	//gameMain *game = new gameMainDevWindow({ .UIScale = 2.0 });
	//gameMain *game = new gameMain("Coin Trap", foo);
	//gameMain *game = new gameMainDevWindow(foo);
	gameMain *game = new gameMain("Coin Trap", foo);

	auto aud = openStereoLoop(DEMO_PREFIX "assets/sfx/jamtest.ogg");
	game->audio->add(aud);

	//game->rend->defaultSkybox = skybox("share/proj/assets/tex/cubes/HeroesSquare/", ".jpg");

	if (auto p = loadSceneCompiled(DEMO_PREFIX "assets/obj/planething.glb")) {
		sceneNode::ptr mod = *p;
		sceneNode::ptr temp = std::make_shared<sceneNode>();
		//sceneNode::ptr probe = std::make_shared<sceneIrradianceProbe>();

		mod->setPosition({ 0, -30, 0 });
		//probe->setPosition({ 0, 10, 0 });
		//setNode("model", temp, mod);
		//setNode("probe", temp, probe);
		game->state->rootnode = temp;

	}

	auto view = std::make_shared<gamething>(game);
	game->setView(view);

	sceneLightDirectional::ptr p = std::make_shared<sceneLightDirectional>();
	p->intensity = 150;
	p->setRotation(glm::vec3 {0, 0, 14*M_PI/4});

	setNode("asdfasdf", game->state->rootnode, p);

	game->run();

	std::cout << "It's alive!" << std::endl;
	return 0;
}
