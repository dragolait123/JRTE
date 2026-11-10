using System;
using System.IO;
using System.Diagnostics;
using JRTE.Core;

string videoPath = args.Length > 0
    ? args[0]
    : @"C:\Users\Utilisateur\Videos\Captures\purityos [En fonction] - Oracle VirtualBox 2026-07-03 16-29-04.mp4";

if (!File.Exists(videoPath))
{
    Console.WriteLine("Fichier MP4 introuvable :");
    Console.WriteLine(videoPath);
    Environment.ExitCode = 1;
    return;
}

Console.WriteLine("====================================");
Console.WriteLine("       JRTE - TEST VIDEO MP4");
Console.WriteLine("====================================");
Console.WriteLine($"Fichier : {videoPath}");

try
{
    using var engine = new NativeVideoEngine();
    engine.Initialize();

    Console.WriteLine("\n===== NOMBRE DE SAMPLES VIDEO =====");

    uint sampleCount = engine.GetVideoSampleCount(videoPath);
    Console.WriteLine($"Total de samples : {sampleCount}");

    Console.WriteLine("\n[1] STRUCTURE MP4");
    Console.WriteLine(engine.InspectMp4(videoPath));

    Console.WriteLine("\n[2] CONFIGURATION AVC / H.264");
    Console.WriteLine(engine.GetAvcConfiguration(videoPath, 0));

    Console.WriteLine("\n[3] ANALYSE DES ECHANTILLONS H.264");

    uint[] samplesToTest = { 0, 1, 2, 10, 100 };

    foreach (uint sampleIndex in samplesToTest)
    {
        Console.WriteLine($"\n--- ECHANTILLON {sampleIndex} ---");

        try
        {
            string report = engine.AnalyzeVideoSample(
                videoPath,
                sampleIndex
            );

            Console.WriteLine(report);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"ERREUR : {ex.Message}");
        }
    }

    Console.WriteLine("\n[4] ANALYSE GLOBALE DE TOUS LES SAMPLES");

    Stopwatch timer = Stopwatch.StartNew();

    try
    {
        string globalReport = engine.AnalyzeAllVideoSamples(videoPath);

        timer.Stop();

        string reportPath = Path.Combine(
            AppContext.BaseDirectory,
            "JRTE_Analyse_Globale.txt"
        );

        File.WriteAllText(
            reportPath,
            globalReport,
            System.Text.Encoding.UTF8
        );

        Console.WriteLine("\nANALYSE GLOBALE TERMINEE !");
        Console.WriteLine($"Temps : {timer.Elapsed}");
        Console.WriteLine($"Rapport : {reportPath}");
        Console.WriteLine($"Taille du rapport : {globalReport.Length:N0} caracteres");
        Console.WriteLine("\n===== RAPPORT GLOBAL =====");
        Console.WriteLine(globalReport);
    }
    catch (Exception ex)
    {
        timer.Stop();

        Console.WriteLine("\nECHEC DE L'ANALYSE GLOBALE");
        Console.WriteLine($"Temps avant erreur : {timer.Elapsed}");
        Console.WriteLine(ex);
        Environment.ExitCode = 1;
    }

    Console.WriteLine("\n====================================");
    Console.WriteLine("       TESTS TERMINES");
    Console.WriteLine("====================================");
}
catch (Exception ex)
{
    Console.WriteLine("\nERREUR DU TEST :");
    Console.WriteLine(ex);
    Environment.ExitCode = 1;
}
