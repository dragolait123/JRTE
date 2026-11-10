using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace JRTE.Core;

public sealed class NativeVideoEngine : IDisposable
{
    private const string LibraryName = "JRTE.VideoEngine";
    private const int MaxSampleSize = 64 * 1024 * 1024;

    [DllImport(LibraryName, EntryPoint = "jrte_get_api_version",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern uint GetApiVersionNative();

    [DllImport(LibraryName, EntryPoint = "jrte_get_engine_name",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr GetEngineNameNative();

    [DllImport(LibraryName, EntryPoint = "jrte_initialize",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern int InitializeNative();

    [DllImport(LibraryName, EntryPoint = "jrte_shutdown",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern void ShutdownNative();

    [DllImport(LibraryName, EntryPoint = "jrte_mp4_inspect",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern int InspectMp4Native(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string utf8Path,
        StringBuilder output,
        uint outputCapacity
    );

    [DllImport(LibraryName,
        EntryPoint = "jrte_mp4_extract_video_sample",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern int ExtractVideoSampleNative(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string utf8Path,
        uint sampleIndex,
        [Out] byte[] output,
        uint outputCapacity,
        out uint bytesWritten
    );
    [DllImport(LibraryName,
        EntryPoint = "jrte_mp4_get_avc_configuration",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern int GetAvcConfigurationNative(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string utf8Path,
        uint sampleIndex,
        StringBuilder output,
        uint outputCapacity
    );

    [DllImport(LibraryName,
        EntryPoint = "jrte_mp4_analyze_video_sample",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern int AnalyzeVideoSampleNative(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string utf8Path,
        uint sampleIndex,
        StringBuilder output,
        uint outputCapacity
    );

    [DllImport(LibraryName,
        EntryPoint = "jrte_mp4_get_video_sample_count",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern int GetVideoSampleCountNative(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string utf8Path,
        out uint sampleCount
    );

    [DllImport(LibraryName,
        EntryPoint = "jrte_mp4_analyze_all_video_samples",
        CallingConvention = CallingConvention.Cdecl)]
    private static extern int AnalyzeAllVideoSamplesNative(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string utf8Path,
        StringBuilder output,
        uint outputCapacity
    );
    public uint ApiVersion => GetApiVersionNative();

    public string EngineName =>
        Marshal.PtrToStringUTF8(GetEngineNameNative())
        ?? "Moteur inconnu";

    public bool IsInitialized { get; private set; }

    public void Initialize()
    {
        if (IsInitialized)
            return;

        int result = InitializeNative();

        if (result != 0)
            throw new InvalidOperationException(
                $"Impossible d'initialiser JRTE. Code : {result}"
            );

        IsInitialized = true;
    }

    public string InspectMp4(string filePath)
    {
        if (!IsInitialized)
            throw new InvalidOperationException(
                "Le moteur JRTE doit Ãªtre initialisÃ© avant l'analyse."
            );

        if (string.IsNullOrWhiteSpace(filePath))
            throw new ArgumentException(
                "Le chemin du fichier est vide.", nameof(filePath)
            );

        if (!File.Exists(filePath))
            throw new FileNotFoundException(
                "Le fichier MP4 est introuvable.", filePath
            );

        var output = new StringBuilder(4096);

        int result = InspectMp4Native(
            filePath,
            output,
            (uint)output.Capacity
        );

        if (result != 0)
        {
            string details = output.ToString();

            throw new InvalidOperationException(
                $"Le parseur MP4 a retournÃ© le code {result}. {details}"
            );
        }

        return output.ToString();
    }

    public byte[] ExtractVideoSample(
        string filePath,
        uint sampleIndex)
    {
        if (!IsInitialized)
            throw new InvalidOperationException(
                "Le moteur JRTE doit Ãªtre initialisÃ©."
            );

        if (string.IsNullOrWhiteSpace(filePath))
            throw new ArgumentException(
                "Chemin invalide.", nameof(filePath)
            );

        if (!File.Exists(filePath))
            throw new FileNotFoundException(
                "Le fichier vidÃ©o est introuvable.", filePath
            );

        byte[] buffer = new byte[MaxSampleSize];

        int result = ExtractVideoSampleNative(
filePath,
            sampleIndex,
            buffer,
            (uint)buffer.Length,
            out uint bytesWritten
        );

        if (result != 0)
        {
            throw new InvalidOperationException(
                $"Extraction Ã©chouÃ©e. Code natif : {result}"
            );
        }

        if (bytesWritten > buffer.Length)
            throw new InvalidOperationException(
                "Le moteur a retournÃ© une taille invalide."
            );

        Array.Resize(
            ref buffer,
            checked((int)bytesWritten)
        );

        return buffer;
    }
public string GetAvcConfiguration(
    string filePath,
    uint sampleIndex)
{
    if (!IsInitialized)
        throw new InvalidOperationException(
            "Le moteur JRTE doit Ãªtre initialisÃ©."
        );

    if (string.IsNullOrWhiteSpace(filePath))
        throw new ArgumentException(
            "Chemin invalide.", nameof(filePath)
        );

    if (!File.Exists(filePath))
        throw new FileNotFoundException(
            "Le fichier vidÃ©o est introuvable.", filePath
        );

    const int OutputCapacity = 8192;

    var output = new StringBuilder(OutputCapacity);

    int result = GetAvcConfigurationNative(
        filePath,
        sampleIndex,
        output,
        OutputCapacity
    );

    if (result != 0)
    {
        throw new InvalidOperationException(
            $"Analyse de la configuration AVC Ã©chouÃ©e. " +
            $"Code natif : {result}"
        );
    }

    return output.ToString();
}
    public string AnalyzeVideoSample(
        string filePath,
        uint sampleIndex)
    {
        if (!IsInitialized)
            throw new InvalidOperationException(
                "Le moteur JRTE doit etre initialise."
            );

        if (string.IsNullOrWhiteSpace(filePath))
            throw new ArgumentException(
                "Chemin invalide.", nameof(filePath)
            );

        if (!File.Exists(filePath))
            throw new FileNotFoundException(
                "Le fichier video est introuvable.", filePath
            );

        const int OutputCapacity = 65536;
        var output = new StringBuilder(OutputCapacity);

        int result = AnalyzeVideoSampleNative(
            filePath,
            sampleIndex,
            output,
            OutputCapacity
        );

        if (result != 0)
        {
            throw new InvalidOperationException(
                $"Analyse de l'echantillon video echouee. Code natif : {result}. " +
                output.ToString()
            );
        }

        return output.ToString();
    }
    public uint GetVideoSampleCount(string filePath)
    {
        if (string.IsNullOrWhiteSpace(filePath))
            throw new ArgumentException("Chemin video invalide.", nameof(filePath));

        if (!File.Exists(filePath))
            throw new FileNotFoundException("Fichier video introuvable.", filePath);

        int result = GetVideoSampleCountNative(filePath, out uint count);

        if (result != 0)
            throw new InvalidOperationException(
                $"Erreur native : {result}");

        return count;
    }

    public string AnalyzeAllVideoSamples(string filePath)
    {
        if (!IsInitialized)
            throw new InvalidOperationException(
                "Le moteur JRTE doit etre initialise.");

        if (string.IsNullOrWhiteSpace(filePath))
            throw new ArgumentException(
                "Chemin video invalide.", nameof(filePath));

        if (!File.Exists(filePath))
            throw new FileNotFoundException(
                "Fichier video introuvable.", filePath);

        const int OutputCapacity = 8 * 1024 * 1024;
        var output = new StringBuilder(OutputCapacity);

        int result = AnalyzeAllVideoSamplesNative(
            filePath,
            output,
            (uint)OutputCapacity);

        if (result != 0)
            throw new InvalidOperationException(
                $"Analyse globale echouee. Code natif : {result}. " +
                output.ToString());

        return output.ToString();
    }
    public void Dispose()
    {
        if (!IsInitialized)
            return;

        ShutdownNative();
        IsInitialized = false;
    }
}



