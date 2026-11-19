#include "NativeVideoEngine.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QByteArray>

#include <vector>

NativeVideoEngine::NativeVideoEngine()
{
}

NativeVideoEngine::~NativeVideoEngine()
{
    shutdown();

    if (library.isLoaded())
        library.unload();
}

bool NativeVideoEngine::load(const QString& dllPath)
{
    if (loaded)
        return true;

    QString path = dllPath;

    if (path.isEmpty())
    {
        path = QDir(
            QCoreApplication::applicationDirPath()
        ).filePath("JRTE.VideoEngine.dll");
    }

    library.setFileName(path);

    if (!library.load())
    {
        error = library.errorString();
        return false;
    }

    getApiVersion = reinterpret_cast<GetApiVersionFn>(
        library.resolve("jrte_get_api_version")
    );

    getEngineName = reinterpret_cast<GetEngineNameFn>(
        library.resolve("jrte_get_engine_name")
    );

    initializeEngine = reinterpret_cast<InitializeFn>(
        library.resolve("jrte_initialize")
    );

    shutdownEngine = reinterpret_cast<ShutdownFn>(
        library.resolve("jrte_shutdown")
    );

    inspectFunction = reinterpret_cast<InspectMp4Fn>(
        library.resolve("jrte_mp4_inspect")
    );

    if (!getApiVersion || !getEngineName ||
        !initializeEngine || !shutdownEngine ||
        !inspectFunction)
    {
        error = "Fonctions natives JRTE introuvables.";
        library.unload();
        return false;
    }

    loaded = true;
    error.clear();
    return true;
}

bool NativeVideoEngine::initialize()
{
    if (!loaded)
    {
        error = "Le moteur natif n'est pas chargé.";
        return false;
    }

    if (initialized)
        return true;

    const int32_t result = initializeEngine();

    if (result != 0)
    {
        error = QString(
            "Initialisation du moteur échouée. Code : %1"
        ).arg(result);
        return false;
    }

    initialized = true;
    return true;
}

QString NativeVideoEngine::inspectMp4(const QString& filePath)
{
    if (!initialized)
    {
        error = "Le moteur n'est pas initialisé.";
        return {};
    }

    const QByteArray utf8Path =
        QFileInfo(filePath).absoluteFilePath().toUtf8();

    constexpr uint32_t capacity = 1024 * 1024;
    std::vector<char> output(capacity, '\0');

    const int32_t result = inspectFunction(
        utf8Path.constData(),
        output.data(),
        capacity
    );

    if (result != 0)
    {
        error = QString(
            "Analyse MP4 échouée. Code natif : %1"
        ).arg(result);
        return {};
    }

    error.clear();
    return QString::fromUtf8(output.data());
}

bool NativeVideoEngine::isLoaded() const
{
    return loaded;
}

bool NativeVideoEngine::isInitialized() const
{
    return initialized;
}

QString NativeVideoEngine::engineName() const
{
    if (!getEngineName)
        return "Moteur non chargé";

    return QString::fromUtf8(getEngineName());
}

QString NativeVideoEngine::lastError() const
{
    return error;
}

void NativeVideoEngine::shutdown()
{
    if (initialized && shutdownEngine)
    {
        shutdownEngine();
        initialized = false;
    }
}
