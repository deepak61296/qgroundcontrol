/****************************************************************************
 *
 * (c) 2024 QGroundControl Project. All rights reserved.
 *
 * AI Chat Controller for ArduPilot AI Backend Integration
 * Handles communication with the AI backend server
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtQmlIntegration/QtQmlIntegration>

class Vehicle;

class AIChatController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString chatHistory READ chatHistory NOTIFY chatHistoryChanged)
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY isProcessingChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)
    Q_PROPERTY(QStringList availableModels READ availableModels NOTIFY availableModelsChanged)
    Q_PROPERTY(QString connectionStatus READ connectionStatus NOTIFY connectionStatusChanged)

public:
    explicit AIChatController(QObject *parent = nullptr);
    ~AIChatController();

    QString chatHistory() const { return _chatHistory; }
    bool isProcessing() const { return _isProcessing; }
    bool isConnected() const { return _isConnected; }
    QStringList availableModels() const { return _availableModels; }
    QString connectionStatus() const { return _connectionStatus; }

    Q_INVOKABLE void sendMessage(const QString &message);
    Q_INVOKABLE void cancelRequest();
    Q_INVOKABLE void checkConnection();
    Q_INVOKABLE void fetchModels();
    Q_INVOKABLE void clearHistory();

signals:
    void chatHistoryChanged();
    void isProcessingChanged();
    void isConnectedChanged();
    void availableModelsChanged();
    void connectionStatusChanged();
    void messageReceived(const QString &response);
    void commandReceived(const QString &commandType, const QJsonObject &params);
    void errorOccurred(const QString &error);

private slots:
    void _onChatReplyFinished();
    void _onHealthReplyFinished();
    void _onModelsReplyFinished();

private:
    void _appendToHistory(const QString &sender, const QString &message, const QString &color);
    QJsonObject _collectTelemetry();
    void _executeCommand(const QString &commandType, const QJsonObject &params);
    QString _getBackendUrl();
    QString _getMode();
    QString _getModel();

    QNetworkAccessManager *_networkManager = nullptr;
    QNetworkReply *_currentReply = nullptr;
    QString _chatHistory;
    bool _isProcessing = false;
    bool _isConnected = false;
    QStringList _availableModels;
    QString _connectionStatus = "Disconnected";
};
