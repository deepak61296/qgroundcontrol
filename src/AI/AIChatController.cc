/****************************************************************************
 *
 * (c) 2024 QGroundControl Project. All rights reserved.
 *
 * AI Chat Controller Implementation
 *
 ****************************************************************************/

#include "AIChatController.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkRequest>
#include <QtPositioning/QGeoCoordinate>
#include <cmath>

#include "AppSettings.h"
#include "MultiVehicleManager.h"
#include "SettingsManager.h"
#include "Vehicle.h"
#include "QmlObjectListModel.h"
#include "FactGroup.h"
#include "FactGroups/BatteryFactGroupListModel.h"

AIChatController::AIChatController(QObject* parent) : QObject(parent), _networkManager(new QNetworkAccessManager(this))
{
    _appendToHistory("System", "AI Assistant ready. Type a message or question.", "#888888");
}

AIChatController::~AIChatController()
{
    cancelRequest();
}

QString AIChatController::_getBackendUrl()
{
    return SettingsManager::instance()->appSettings()->aiBackendUrl()->rawValue().toString();
}

QString AIChatController::_getMode()
{
    int modeIndex = SettingsManager::instance()->appSettings()->aiBackendMode()->rawValue().toInt();
    return modeIndex == 0 ? "ask" : "agent";
}

QString AIChatController::_getModel()
{
    return SettingsManager::instance()->appSettings()->aiBackendModel()->rawValue().toString();
}

void AIChatController::sendMessage(const QString& message)
{
    if (_isProcessing || message.trimmed().isEmpty()) {
        return;
    }

    _isProcessing = true;
    emit isProcessingChanged();

    // Add user message to history
    _appendToHistory("You", message, "#0078D7");

    // Prepare request
    QString url = _getBackendUrl() + "/chat";
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(60000);  // 60 second timeout

    // Build JSON payload
    QJsonObject payload;
    payload["message"] = message;
    payload["mode"] = _getMode();
    payload["model"] = _getModel();
    payload["telemetry"] = _collectTelemetry();

    QJsonDocument doc(payload);
    QByteArray data = doc.toJson();

    // Send request
    _currentReply = _networkManager->post(request, data);
    connect(_currentReply, &QNetworkReply::finished, this, &AIChatController::_onChatReplyFinished);
}

void AIChatController::_onChatReplyFinished()
{
    if (!_currentReply) {
        return;
    }

    _isProcessing = false;
    emit isProcessingChanged();

    if (_currentReply->error() != QNetworkReply::NoError) {
        QString errorMsg = _currentReply->errorString();
        _appendToHistory("System", "Error: " + errorMsg, "#FF6B6B");
        emit errorOccurred(errorMsg);
        _currentReply->deleteLater();
        _currentReply = nullptr;
        return;
    }

    QByteArray responseData = _currentReply->readAll();
    _currentReply->deleteLater();
    _currentReply = nullptr;

    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (!doc.isObject()) {
        _appendToHistory("System", "Error: Invalid response from backend", "#FF6B6B");
        return;
    }

    QJsonObject response = doc.object();

    if (!response["success"].toBool()) {
        QString error = response["error"].toString("Unknown error");
        _appendToHistory("System", "Error: " + error, "#FF6B6B");
        emit errorOccurred(error);
        return;
    }

    // Display AI response
    QString aiResponse = response["response"].toString();
    if (!aiResponse.isEmpty()) {
        _appendToHistory("AI", aiResponse, "#00C853");
        emit messageReceived(aiResponse);
    }

    // Handle command if present (Agent mode)
    if (response.contains("command") && !response["command"].isNull()) {
        QJsonObject command = response["command"].toObject();
        QString commandType = command["type"].toString();
        QJsonObject params = command["params"].toObject();

        if (!commandType.isEmpty()) {
            _appendToHistory("System", "Executing: " + commandType, "#FFA500");
            _executeCommand(commandType, params);
            emit commandReceived(commandType, params);
        }
    }
}

void AIChatController::cancelRequest()
{
    if (_currentReply) {
        _currentReply->abort();
        _currentReply->deleteLater();
        _currentReply = nullptr;
    }
    _isProcessing = false;
    emit isProcessingChanged();
}

void AIChatController::checkConnection()
{
    QString url = _getBackendUrl() + "/health";
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    request.setTransferTimeout(5000);  // 5 second timeout

    QNetworkReply* reply = _networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &AIChatController::_onHealthReplyFinished);
}

void AIChatController::_onHealthReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    bool wasConnected = _isConnected;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        _isConnected = obj["status"].toString() == "healthy";
        _connectionStatus = _isConnected ? "Connected" : "Unhealthy";
    } else {
        _isConnected = false;
        _connectionStatus = "Disconnected";
    }

    reply->deleteLater();

    if (wasConnected != _isConnected) {
        emit isConnectedChanged();
    }
    emit connectionStatusChanged();
}

void AIChatController::fetchModels()
{
    // Query Ollama directly for available models
    QString url = "http://localhost:11434/api/tags";
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    request.setTransferTimeout(5000);

    QNetworkReply* reply = _networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &AIChatController::_onModelsReplyFinished);
}

void AIChatController::_onModelsReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    _availableModels.clear();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        QJsonArray models = obj["models"].toArray();

        for (const QJsonValue& value : models) {
            QJsonObject model = value.toObject();
            QString name = model["name"].toString();
            if (!name.isEmpty()) {
                _availableModels.append(name);
            }
        }
    }

    // Fallback to default models if Ollama not available
    if (_availableModels.isEmpty()) {
        _availableModels << "qwen2.5:3b" << "qwen2.5:7b";
    }

    reply->deleteLater();
    emit availableModelsChanged();
}

void AIChatController::clearHistory()
{
    _chatHistory.clear();
    _appendToHistory("System", "Chat history cleared.", "#888888");
    emit chatHistoryChanged();
}

void AIChatController::_appendToHistory(const QString& sender, const QString& message, const QString& color)
{
    QString formattedMsg =
        QString("<p style=\"margin:4px 0;\"><span style=\"color:%1;font-weight:bold;\">%2:</span> %3</p>")
            .arg(color, sender, message.toHtmlEscaped());
    _chatHistory += formattedMsg;
    emit chatHistoryChanged();
}

QJsonObject AIChatController::_collectTelemetry()
{
    QJsonObject telemetry;

    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (!vehicle) {
        return telemetry;
    }

    // Battery info (use first battery if available)
    QJsonObject battery;
    QmlObjectListModel* batteries = vehicle->batteries();
    if (batteries && batteries->count() > 0) {
        BatteryFactGroup* batteryGroup = qobject_cast<BatteryFactGroup*>(batteries->get(0));
        if (batteryGroup) {
            battery["voltage"] = batteryGroup->voltage()->rawValue().toDouble();
            battery["remaining"] = batteryGroup->percentRemaining()->rawValue().toInt();
        }
    }
    telemetry["battery"] = battery;

    // GPS info
    QJsonObject gps;
    gps["latitude"] = vehicle->latitude();
    gps["longitude"] = vehicle->longitude();
    gps["altitude"] = vehicle->altitudeRelative()->rawValue().toDouble();
    FactGroup* gpsFactGroup = vehicle->gpsFactGroup();
    if (gpsFactGroup) {
        Fact* countFact = gpsFactGroup->getFact("count");
        if (countFact) {
            gps["satellites"] = countFact->rawValue().toInt();
        }
    }
    telemetry["gps"] = gps;

    // Status
    QJsonObject status;
    status["mode"] = vehicle->flightMode();
    status["armed"] = vehicle->armed();
    telemetry["status"] = status;

    // Attitude
    QJsonObject attitude;
    attitude["roll"] = vehicle->roll()->rawValue().toDouble();
    attitude["pitch"] = vehicle->pitch()->rawValue().toDouble();
    attitude["yaw"] = vehicle->heading()->rawValue().toDouble();
    telemetry["attitude"] = attitude;

    // Speed
    QJsonObject speed;
    speed["ground_speed"] = vehicle->groundSpeed()->rawValue().toDouble();
    speed["climb_rate"] = vehicle->climbRate()->rawValue().toDouble();
    telemetry["speed"] = speed;

    return telemetry;
}

void AIChatController::_executeCommand(const QString& commandType, const QJsonObject& params)
{
    Vehicle* vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (!vehicle) {
        _appendToHistory("System", "Error: No active vehicle", "#FF6B6B");
        return;
    }

    // Execute commands based on type
    if (commandType == "ARM") {
        vehicle->setArmed(true, false);
    } else if (commandType == "DISARM") {
        vehicle->setArmed(false, false);
    } else if (commandType == "TAKEOFF") {
        double altitude = params["altitude"].toDouble(10.0);
        vehicle->guidedModeTakeoff(altitude);
    } else if (commandType == "LAND") {
        vehicle->guidedModeLand();
    } else if (commandType == "RTL") {
        vehicle->guidedModeRTL(true);
    } else if (commandType == "CHANGE_MODE") {
        QString mode = params["mode"].toString();
        vehicle->setFlightMode(mode);
    } else if (commandType == "GOTO") {
        double lat = params["latitude"].toDouble();
        double lon = params["longitude"].toDouble();
        double alt = params["altitude"].toDouble();
        QGeoCoordinate coord(lat, lon, alt);
        vehicle->guidedModeGotoLocation(coord);
    } else if (commandType == "MOVE_DIRECTION") {
        QString direction = params["direction"].toString().toUpper();
        double distance = params["distance"].toDouble(10.0);

        // Get current position
        double lat = vehicle->latitude();
        double lon = vehicle->longitude();
        double alt = vehicle->altitudeRelative()->rawValue().toDouble();

        // Calculate offset based on direction (approximate meters to degrees)
        double latOffset = 0.0;
        double lonOffset = 0.0;
        double metersPerDegreeLat = 111320.0;
        double metersPerDegreeLon = 111320.0 * cos(lat * M_PI / 180.0);

        if (direction == "NORTH") {
            latOffset = distance / metersPerDegreeLat;
        } else if (direction == "SOUTH") {
            latOffset = -distance / metersPerDegreeLat;
        } else if (direction == "EAST") {
            lonOffset = distance / metersPerDegreeLon;
        } else if (direction == "WEST") {
            lonOffset = -distance / metersPerDegreeLon;
        }

        QGeoCoordinate newCoord(lat + latOffset, lon + lonOffset, alt);
        vehicle->guidedModeGotoLocation(newCoord);
    } else {
        _appendToHistory("System", "Command not implemented: " + commandType, "#FFA500");
    }
}
