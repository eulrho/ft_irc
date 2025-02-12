#include <algorithm>
#include <cctype>
#include "../include/Server.hpp"

std::string Server::setPassword(Request &request, int fd)
{
	// 순서가 항상 pass -> nick -> user 로 고정이여야만 하는 지 확인해야함
	if (request.args.size() < 1)
		return Response::failure(ERR_NEEDMOREPARAMS, "PASS", this->name, this->clients[fd]->getNickname());
	if (this->clients[fd]->getIsRegistered())
		return Response::failure(ERR_ALREADYREGISTRED, "", this->name, this->clients[fd]->getNickname());
	if (this->password == request.args[0])
		this->clients[fd]->setIsValidPasswd(true);
	else
		this->clients[fd]->setIsValidPasswd(false);
	return "";
}

std::string Server::setUserNickname(Request &request, int fd)
{
	std::string nickname = this->clients[fd]->getNickname();
	std::string sendMsg;

	if (request.args.size() < 1)
		return Response::failure(ERR_NONICKNAMEGIVEN, "", this->name, nickname);
	std::string inputNickname = request.args[0];
	if (isSameNickname(inputNickname, nickname))
		return "";
	if (!isValidUserNickname(inputNickname))
		return Response::failure(ERR_ERRONEUSNICKNAME, inputNickname, this->name, nickname);
	if (isUsedUserNickname(inputNickname)) {
		if (this->clients[fd]->getIsFirstLogin() == false)
			return Response::failure(ERR_NICKNAMEINUSE, inputNickname, this->name, nickname);
		while (isUsedUserNickname(inputNickname))
			inputNickname += "_";
		this->clients[fd]->setIsFirstLogin(false);
	}
	if (this->clients[fd]->getIsValidPasswd()) {
		if (nickname != "")
			deleteUserNickname(nickname);
		this->clients[fd]->setNickname(inputNickname);
		addNewUserNickname(inputNickname);
	}
	sendMsg = Response::success(RPL_WELCOME, this->name, this->clients[fd]->getPrefix(), this->clients[fd]->getNickname(), this->clients[fd]->getRealName());
	send(fd, sendMsg.c_str(), sendMsg.length(), 0);
	sendMsg = Response::success(RPL_CHANGEDNICK, this->name, this->clients[fd]->getPrefix(), this->clients[fd]->getNickname(), inputNickname);
	send(fd, sendMsg.c_str(), sendMsg.length(), 0);
	return "";
}

std::string Server::setUser(Request &request, int fd)
{
	if (request.args.size() < 4)
		return Response::failure(ERR_NEEDMOREPARAMS, "USER", this->name, this->clients[fd]->getNickname());
	if (this->clients[fd]->getIsRegistered())
		return Response::failure(ERR_ALREADYREGISTRED, "", this->name, this->clients[fd]->getNickname());
	if (this->clients[fd]->getNickname() != "") {
		this->clients[fd]->setUserName(request.args[0]);
		this->clients[fd]->setRealName(request.args[3]);
		this->clients[fd]->setPrefix();
		this->clients[fd]->setIsRegistered(true);
	}
	return "";
}

//getPrefix() 대신 this->name 사용
std::string Server::getFile(Request &request, int fd)
{
	if (request.args.size() < 2)
		return (Response::failure(ERR_NEEDMOREPARAMS, "GETFILE", this->name, this->clients[fd]->getNickname()));
	if (!this->clients[fd]->getIsRegistered())
		return (Response::failure(ERR_NOTREGISTERED, "", this->name, this->clients[fd]->getNickname()));

	std::string channelName = request.args[0];
	std::string fileName = request.args[1];
	if (this->channels.find(channelName) == this->channels.end())
		return (Response::failure(ERR_NOSUCHCHANNEL, channelName, this->name, this->clients[fd]->getNickname()));
	if (!this->channels[channelName]->findFile(fileName))
		return (Response::failure(ERR_FILENOTFOUND, fileName, this->name, this->clients[fd]->getNickname()));

	File file = this->channels[channelName]->getFiles()[fileName];
	std::fstream ofs((this->getDownloadPath() + fileName).c_str(), std::fstream::out);
	if (ofs.fail())
		return (Response::failure(ERR_FILEERROR, fileName, this->name, this->clients[fd]->getNickname()));
	ofs << file.getFileContent();
	ofs.close();
	return Response::success(RPL_FILEDELIVERED, channelName, this->name, this->clients[fd]->getNickname(), fileName);
}

std::string Server::sendFile(Request &request, int fd)
{
	if (request.args.size() < 2)
		return (Response::failure(ERR_NEEDMOREPARAMS, "SENDFILE", this->name, this->clients[fd]->getNickname()));
	if (!this->clients[fd]->getIsRegistered())
		return (Response::failure(ERR_NOTREGISTERED, "", this->name, this->clients[fd]->getNickname()));

	std::string channelName = request.args[0];
	std::string filePath = request.args[1];
	if (this->channels.find(channelName) == this->channels.end())
		return (Response::failure(ERR_NOSUCHCHANNEL, channelName, this->name, this->clients[fd]->getNickname()));
	std::fstream ifs(request.args[1].c_str(), std::fstream::in);
	if (ifs.fail())
		return (Response::failure(ERR_INVALIDFILEPATH, request.args[1], this->name, this->clients[fd]->getNickname()));
	std::string fileName = request.args[1].substr(request.args[1].find_last_of('/') + 1);
	std::cout << "fileName: " << fileName << std::endl; //erase
	File file(fileName, channelName);
	if (this->channels[channelName]->findFile(fileName))
		return (Response::failure(ERR_FILEERROR, fileName, this->name, this->clients[fd]->getNickname()));
	std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
	this->channels[channelName]->addFile(fileName, content);
	ifs.close();
	return Response::success(RPL_FILESENT, channelName, this->name, this->clients[fd]->getNickname(), fileName);
}

std::string Server::quit(Request &request, int fd)
{
	std::string reason = "Client exited";

	if (!request.args.empty()) {
		int size = static_cast<int>(request.args.size());
		reason = "Quit: ";
		for (int i = 0; i < size; i++) {
			reason += request.args[i];
			if (i + 1 < size) reason += " ";
		}
	}

	std::cout << "test" << std::endl;
	std::string user = this->clients[fd]->getPrefix().substr(this->clients[fd]->getNickname().size() + 1);
	std::cout << "test done" << std::endl;
	std::string res = Response::customErrorMessageForQuit(user, reason);
	send(fd, res.c_str(), res.length(), 0);

	this->removeClient(fd, reason);
	this->removeClientConnection(fd);
	close(fd);
	return "";
}

void Server::quit(int fd)
{
	std::string reason = "Connection closed";
	std::cout << reason << std::endl;
	if (this->clients.find(fd) != this->clients.end())
		this->removeClient(fd, reason);
	this->removeClientConnection(fd);
	close(fd);
}

std::string Server::joinChannel(Request &request, int fd)
{
	if (!this->clients[fd]->getIsRegistered())
		return Response::failure(ERR_NOTREGISTERED, "", this->name, this->clients[fd]->getNickname());
	if (request.args.size() < 1)
		return Response::failure(ERR_NEEDMOREPARAMS, "JOIN", this->name, this->clients[fd]->getNickname());

	std::vector<std::string> channelName;
	std::vector<std::string> keys;
	std::string res;
	makeVector(request.args[0], channelName);
	if (request.args.size() > 1)
		makeVector(request.args[1], keys);
	for (size_t i = 0; i < channelName.size(); i++) {
		ErrorCode err = join(channelName[i], keys.size() > i ? keys[i] : "", fd);
		if (err != ERR_NONE)
			sendError(err, channelName[i], fd);
	}
	return "";
}

std::string Server::partChannel(Request &request, int fd)
{
	if (request.args.size() < 1)
		return Response::failure(ERR_NEEDMOREPARAMS, "PART", this->name, this->clients[fd]->getNickname());
	if (!this->clients[fd]->getIsRegistered())
		return Response::failure(ERR_NOTREGISTERED, "", this->name, this->clients[fd]->getNickname());
	
	std::vector<std::string> channelName;
	std::string res;
	makeVector(request.args[0], channelName);

	for (size_t i = 0; i < channelName.size(); i++) {
		ErrorCode err = part(channelName[i], fd);
		if (err != ERR_NONE)
			sendError(err, channelName[i], fd);
	}
	return "";
}

std::string Server::kickUser(Request &request, int fd)
{
	if (request.args.size() < 2)
		return Response::failure(ERR_NEEDMOREPARAMS, "KICK", this->name, this->clients[fd]->getNickname());
	if (!this->clients[fd]->getIsRegistered())
		return Response::failure(ERR_NOTREGISTERED, "", this->name, this->clients[fd]->getNickname());
	
	std::vector<std::string> channelName;
	std::vector<std::string> nicknames;
	std::string reason;
	makeVector(request.args[0], channelName);
	makeVector(request.args[1], nicknames);
	size_t size = request.args.size();
	for (size_t i = 2; i < size; i++) {
		reason += request.args[i];
		if (i + 1 < size) reason += " ";
	}
	if (reason.empty())
		reason = this->clients[fd]->getNickname();
	for (size_t i = 0; i < channelName.size(); i++) {
		ErrorCode err = kick(channelName[i], nicknames.size() > i ? nicknames[i] : "", reason, fd);
		if (err != ERR_NONE)
			sendError(err, channelName[i], fd);
	}
	return "";
}

std::string Server::inviteUser(Request &request, int fd)
{
	if (request.args.size() < 2)
		return Response::failure(ERR_NEEDMOREPARAMS, "INVITE", this->name, this->clients[fd]->getNickname());
	if (!this->clients[fd]->getIsRegistered())
		return Response::failure(ERR_NOTREGISTERED, "", this->name, this->clients[fd]->getNickname());

	std::string channelName = request.args[0];
	if (this->channels.find(channelName) == this->channels.end())
		return Response::failure(ERR_NOSUCHCHANNEL, channelName, this->name, this->clients[fd]->getNickname());
	if (!this->channels[channelName]->isMember(fd))
		return Response::failure(ERR_NOTONCHANNEL, channelName, this->name, this->clients[fd]->getNickname());
	if (!this->channels[channelName]->isOperator(fd))
		return Response::failure(ERR_CHANOPRIVSNEEDED, channelName, this->name, this->clients[fd]->getNickname());
	
	std::string invitedNickname = request.args[1];
	if (this->isClientInServer(invitedNickname) == false)
		return Response::failure(ERR_NOSUCHNICK, invitedNickname, this->name, this->clients[fd]->getNickname());
	
	int invitedFd = this->getClientByNickname(invitedNickname)->getClientFd();
	if (this->channels[channelName]->isMember(invitedFd))
		return Response::failure(ERR_USERONCHANNEL, invitedNickname, this->name, this->clients[fd]->getNickname());
	this->clients[invitedFd]->addInvitedChannel(channelName);

	std::string response = Response::customMessageForInvite(this->clients[fd]->getPrefix(), channelName, invitedNickname);
	send(invitedFd, response.c_str(), response.length(), 0);
	return Response::success(RPL_INVITING, channelName, this->name, this->clients[fd]->getNickname(), invitedNickname);
}

//mode #1 +k+t-t aaa bcd <seg>
//mode 전부 실패했을 시, 성공 sendMsg 보내면 안됨
//mode #1 -k aaa <seg>
std::string Server::setMode(Request &request, int fd)
{
	size_t size = request.args.size();
	if (size < 1)
		return Response::failure(ERR_NEEDMOREPARAMS, "MODE", this->name, this->clients[fd]->getNickname());
	if (!this->clients[fd]->getIsRegistered())
		return Response::failure(ERR_NOTREGISTERED, "", this->name, this->clients[fd]->getNickname());
	std::string channelName = request.args[0];
	if (this->channels.find(channelName) == this->channels.end())
		return Response::failure(ERR_NOSUCHCHANNEL, channelName, this->name, this->clients[fd]->getNickname());
	if (!this->channels[channelName]->isMember(fd))
		return Response::failure(ERR_NOTONCHANNEL, channelName, this->name, this->clients[fd]->getNickname());
	if (size == 1)
		return modeInfo(this->channels[channelName], fd);
	if (!this->channels[channelName]->isOperator(fd))
		return Response::failure(ERR_CHANOPRIVSNEEDED, channelName, this->name, this->clients[fd]->getNickname());
	
	std::string sendMsg;
	classifyMode(request, sendMsg, fd);
	return Response::customMessageForMode(this->clients[fd]->getPrefix(), channelName, sendMsg);
}

std::string Server::topic(Request &request, int fd)
{
	size_t size = request.args.size();

	if (!this->clients[fd]->getIsRegistered())
		return Response::failure(ERR_NOTREGISTERED, "", this->name, this->clients[fd]->getNickname());
	if (size < 1)
		return Response::failure(ERR_NEEDMOREPARAMS, "TOPIC", this->name, this->clients[fd]->getNickname());

	std::string channelName = request.args[0];

	if (this->channels.find(channelName) == this->channels.end())
		return Response::failure(ERR_NOSUCHCHANNEL, channelName, this->name, this->clients[fd]->getNickname());
	if (!this->channels[channelName]->isMember(fd))
		return Response::failure(ERR_NOTONCHANNEL, channelName, this->name, this->clients[fd]->getNickname());
	
	// notice topic to user
	if (size == 1) {
		std::string topic = this->channels[channelName]->getTopic();
		if (topic.empty())
			return Response::success(RPL_NOTOPIC, channelName, this->clients[fd]->getPrefix(), this->clients[fd]->getNickname(), "");
		return Response::success(RPL_TOPIC, channelName, this->clients[fd]->getPrefix(), this->clients[fd]->getNickname(), topic);
	}
	
	// set new topic and notice to users in the channel
	if (this->channels[channelName]->getIsTopicChangeByOperatorOnly() && !this->channels[channelName]->isOperator(fd))
		return Response::failure(ERR_CHANOPRIVSNEEDED, channelName, this->name, this->clients[fd]->getNickname());

	std::string newTopic = request.args[1];
	this->channels[channelName]->setTopic(newTopic);
	broadcastChannel(channelName, Response::success(RPL_TOPIC, channelName, this->clients[fd]->getPrefix(), this->clients[fd]->getNickname(), this->channels[channelName]->getTopic()));
	return "";
}

std::string Server::sendPrivmsg(Request &request, int fd)
{
	size_t size = request.args.size();
	if (size < 2)
		return Response::failure(ERR_NEEDMOREPARAMS, "PRIVMSG", this->name, this->clients[fd]->getNickname());
	if (!this->clients[fd]->getIsRegistered())
		return Response::failure(ERR_NOTREGISTERED, "", this->name, this->clients[fd]->getNickname());

	std::vector<std::string> target;
	makeVector(request.args[0], target);
	std::string message = request.args[1];

	for (size_t i = 0; i < target.size(); i++) {
		if (target[i][0] == '#' || target[i][0] == '&') {
			ErrorCode err = privmsgToChannel(target[i], message, fd);
			if (err != ERR_NONE)
				sendError(err, target[i], fd);
		}
		else {
			ErrorCode err = privmsgToUser(target[i], message, fd);
			if (err != ERR_NONE)
				sendError(err, target[i], fd);
		}
	}
	return "";
}