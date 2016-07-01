/**
 * @file
 */

#include "Server.h"
#include "core/Var.h"
#include "core/Command.h"
#include "backend/storage/StoreCmd.h"
#include "backend/network/ServerNetworkModule.h"
#include "ServerModule.h"
#include <cstdlib>

Server::Server(network::NetworkPtr network, backend::ServerLoopPtr serverLoop, core::TimeProviderPtr timeProvider, io::FilesystemPtr filesystem, core::EventBusPtr eventBus) :
		core::App(filesystem, eventBus, 15678), _quit(false), _network(network), _serverLoop(serverLoop), _timeProvider(timeProvider) {
	init("engine", "server");
}

core::AppState Server::onInit() {
	const core::AppState state = core::App::onInit();
	if (!_network->start()) {
		Log::error("Failed to start the server");
		return core::Cleanup;
	}

	if (!_serverLoop->onInit()) {
		Log::error("Failed to init the main loop");
		return core::Cleanup;
	}

	core::Command::registerCommand("quit", [&] (const core::CmdArgs& args) {_quit = true;});
	backend::StoreCmd SCmd;
	SCmd.addComd();

	const core::VarPtr& port = core::Var::get(cfg::ServerPort, "11337");
	const core::VarPtr& host = core::Var::get(cfg::ServerHost, "");
	const core::VarPtr& maxclients = core::Var::get(cfg::ServerMaxClients, "1024");
	if (!_network->bind(port->intVal(), host->strVal(), maxclients->intVal(), 2)) {
		Log::error("Failed to bind the server socket on %s:%i", host->strVal().c_str(), port->intVal());
		return core::Cleanup;
	}
	Log::info("Server socket is up at %s:%i", host->strVal().c_str(), port->intVal());
	return state;
}

core::AppState Server::onRunning() {
	if (_quit)
		return core::Cleanup;
	_timeProvider->update(_now);
	core::App::onRunning();
	_serverLoop->onFrame(_deltaFrame);
	return core::Running;
}

int main(int argc, char *argv[]) {
	return core::getAppWithModules<Server>(ServerModule(), backend::ServerNetworkModule())->startMainLoop(argc, argv);
}
