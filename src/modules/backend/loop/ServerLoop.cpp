/**
 * @file
 */

#include "ClientMessages_generated.h"
#include "ServerLoop.h"

#include "core/command/Command.h"
#include "core/Log.h"
#include "core/App.h"
#include "io/Filesystem.h"
#include "cooldown/CooldownProvider.h"
#include "persistence/ConnectionPool.h"
#include "BackendModels.h"
#include "EventMgrModels.h"
#include "backend/entity/User.h"
#include "backend/entity/ai/AIRegistry.h"
#include "backend/entity/ai/AICommon.h"
#include "backend/network/UserConnectHandler.h"
#include "backend/network/UserConnectedHandler.h"
#include "backend/network/UserDisconnectHandler.h"
#include "backend/network/AttackHandler.h"
#include "backend/network/MoveHandler.h"
#include "core/command/CommandHandler.h"
#include "voxel/MaterialColor.h"
#include "eventmgr/EventMgr.h"
#include "stock/StockDataProvider.h"
#include "metric/UDPMetricSender.h"

namespace backend {

constexpr int aiDebugServerPort = 11338;
constexpr const char* aiDebugServerInterface = "127.0.0.1";

ServerLoop::ServerLoop(const persistence::DBHandlerPtr& dbHandler, const network::ServerNetworkPtr& network,
		const SpawnMgrPtr& spawnMgr, const voxel::WorldPtr& world,  const EntityStoragePtr& entityStorage,
		const core::EventBusPtr& eventBus, const AIRegistryPtr& registry,
		const attrib::ContainerProviderPtr& containerProvider, const poi::PoiProviderPtr& poiProvider,
		const cooldown::CooldownProviderPtr& cooldownProvider, const eventmgr::EventMgrPtr& eventMgr,
		const stock::StockProviderPtr& stockDataProvider) :
		_network(network), _spawnMgr(spawnMgr), _world(world),
		_entityStorage(entityStorage), _eventBus(eventBus), _registry(registry), _attribContainerProvider(containerProvider),
		_poiProvider(poiProvider), _cooldownProvider(cooldownProvider), _eventMgr(eventMgr), _dbHandler(dbHandler),
		_stockDataProvider(stockDataProvider), _metric(std::make_shared<metric::UDPMetricSender>(), "server.") {
	_world->setClientData(false);
	_eventBus->subscribe<network::NewConnectionEvent>(*this);
	_eventBus->subscribe<network::DisconnectEvent>(*this);
	_eventBus->subscribe<metric::MetricEvent>(*this);
}

#define regHandler(type, handler, ...) \
	r->registerHandler(network::EnumNameClientMsgType(type), std::make_shared<handler>(__VA_ARGS__));

bool ServerLoop::init() {
	const io::FilesystemPtr& filesystem = core::App::getInstance()->filesystem();
	if (!_metric.init()) {
		Log::warn("Failed to init statsd metrics");
	}
	if (!_dbHandler->init()) {
		Log::error("Failed to init the dbhandler");
		return false;
	}
	if (!_dbHandler->createTable(db::UserModel())) {
		Log::error("Failed to create user table");
		return false;
	}
	if (!_dbHandler->createTable(db::StockModel())) {
		Log::error("Failed to create stock table");
		return false;
	}
	if (!_dbHandler->createTable(db::InventoryModel())) {
		Log::error("Failed to create stock table");
		return false;
	}
	if (!_eventMgr->init()) {
		Log::error("Failed to init event manager");
		return false;
	}

	const std::string& cooldowns = filesystem->load("cooldowns.lua");
	if (!_cooldownProvider->init(cooldowns)) {
		Log::error("Failed to load the cooldown configuration: %s", _cooldownProvider->error().c_str());
		return false;
	}

	const std::string& stockLuaString = filesystem->load("stock.lua");
	if (!_stockDataProvider->init(stockLuaString)) {
		Log::error("Failed to load the stock configuration: %s", _stockDataProvider->error().c_str());
		return false;
	}

	const std::string& attributes = filesystem->load("attributes.lua");
	if (!_attribContainerProvider->init(attributes)) {
		Log::error("Failed to load the attributes: %s", _attribContainerProvider->error().c_str());
		return false;
	}

	_zone = new ai::Zone("Zone");
	_aiServer = new ai::Server(*_registry, aiDebugServerPort, aiDebugServerInterface);
	_registry->init(_spawnMgr);
	if (!_spawnMgr->init()) {
		Log::error("Failed to init the spawn manager");
		return false;
	}

	const network::ProtocolHandlerRegistryPtr& r = _network->registry();
	regHandler(network::ClientMsgType::UserConnect, UserConnectHandler, _network, _entityStorage, _world);
	regHandler(network::ClientMsgType::UserConnected, UserConnectedHandler);
	regHandler(network::ClientMsgType::UserDisconnect, UserDisconnectHandler);
	regHandler(network::ClientMsgType::Attack, AttackHandler);
	regHandler(network::ClientMsgType::Move, MoveHandler);

	if (!voxel::initDefaultMaterialColors()) {
		Log::error("Failed to initialize the palette data");
		return false;
	}

	if (!_world->init(filesystem->load("world.lua"), filesystem->load("biomes.lua"))) {
		Log::error("Failed to init the world");
		return false;
	}

	const core::VarPtr& seed = core::Var::getSafe(cfg::ServerSeed);
	_world->setSeed(seed->longVal());
	_world->setPersist(false);
	if (_aiServer->start()) {
		Log::info("Start the ai debug server on %s:%i", aiDebugServerInterface, aiDebugServerPort);
		_aiServer->addZone(_zone);
	} else {
		Log::error("Could not start the ai debug server");
	}
	return true;
}

void ServerLoop::shutdown() {
	_world->shutdown();
	_spawnMgr->shutdown();
	_dbHandler->shutdown();
	delete _zone;
	delete _aiServer;
	_zone = nullptr;
	_aiServer = nullptr;
	_metric.shutdown();
}

void ServerLoop::readInput() {
	const char *input = _input.read();
	if (input == nullptr) {
		return;
	}

	core::executeCommands(input);
}

void ServerLoop::onFrame(long dt) {
	readInput();
	core_trace_scoped(ServerLoop);
	core::Var::visitReplicate([] (const core::VarPtr& var) {
		Log::info("TODO: %s needs replicate", var->name().c_str());
	});
	_network->update();
	{ // TODO: move into own thread
		core_trace_scoped(PoiUpdate);
		_poiProvider->update(dt);
	}
	{ // TODO: move into own thread
		core_trace_scoped(WorldUpdate);
		_world->onFrame(dt);
	}
	{ // TODO: move into own thread
		core_trace_scoped(AIServerUpdate);
		_zone->update(dt);
		_aiServer->update(dt);
	}
	{ // TODO: move into own thread
		core_trace_scoped(SpawnMgrUpdate);
		_spawnMgr->onFrame(*_zone, dt);
	}
	{
		core_trace_scoped(EntityStorage);
		_entityStorage->onFrame(dt);
	}
	_metric.timing("frame.delta", dt);
}

void ServerLoop::onEvent(const metric::MetricEvent& event) {
	metric::MetricEventType type = event.type();
	switch (type) {
	case metric::MetricEventType::Count:
		_metric.count(event.key().c_str(), event.value(), event.tags());
		break;
	case metric::MetricEventType::Gauge:
		_metric.gauge(event.key().c_str(), (uint32_t)event.value(), event.tags());
		break;
	case metric::MetricEventType::Timing:
		_metric.timing(event.key().c_str(), (uint32_t)event.value(), event.tags());
		break;
	case metric::MetricEventType::Histogram:
		_metric.histogram(event.key().c_str(), (uint32_t)event.value(), event.tags());
		break;
	case metric::MetricEventType::Meter:
		_metric.meter(event.key().c_str(), event.value(), event.tags());
		break;
	}
}

void ServerLoop::onEvent(const network::DisconnectEvent& event) {
	ENetPeer* peer = event.peer();
	Log::info("disconnect peer: %u", peer->connectID);
	User* user = reinterpret_cast<User*>(peer->data);
	if (user == nullptr) {
		return;
	}
	// TODO: handle this and abort on re-login
	user->cooldownMgr().triggerCooldown(cooldown::Type::LOGOUT);
	_metric.decrement("count.user");
}

void ServerLoop::onEvent(const network::NewConnectionEvent& event) {
	Log::info("new connection - waiting for login request from %u", event.peer()->connectID);
	_metric.increment("count.user");
}

}
