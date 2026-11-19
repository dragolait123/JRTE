#pragma once

#include <QString>
#include <QLibrary>
#include <cstdint>

class NativeVideoEngine final
{
public:
    NativeVideoEngine();
    ~NativeVideoEngine();

    NativeVideoEngine(const NativeVideoEngine&) = delete;
    NativeVideoEngine& operator=(const NativeVideoEngine&) = delete;

    bool load(const QString& dllPath = QString());
    bool initialize();
    QString inspectMp4(const QString& filePath);

    bool isLoaded() const;
    bool isInitialized() const;

    QString engineName() const;
    QString lastError() const;

    void shutdown();

private:
    using GetApiVersionFn = uint32_t (*)();
    using GetEngineNameFn = const char* (*)();
    using InitializeFn = int32_t (*)();
    using ShutdownFn = void (*)();
    using InspectMp4Fn = int32_t (*)(const char*, char*, uint32_t);

    QLibrary library;

    GetApiVersionFn getApiVersion = nullptr;
    GetEngineNameFn getEngineName = nullptr;
    InitializeFn initializeEngine = nullptr;
    ShutdownFn shutdownEngine = nullptr;
    InspectMp4Fn inspectFunction = nullptr;

    bool loaded = false;
    bool initialized = false;

    QString error;
};
