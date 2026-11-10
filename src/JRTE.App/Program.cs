using JRTE.Core;

Console.WriteLine("================================");
Console.WriteLine("       JRTE VIDEO ENGINE");
Console.WriteLine("================================");

try
{
    using var engine = new NativeVideoEngine();

    Console.WriteLine($"Moteur : {engine.EngineName}");
    Console.WriteLine($"API native : {engine.ApiVersion}");

    engine.Initialize();

    Console.WriteLine("Initialisation : REUSSIE");
    Console.WriteLine("JRTE C# <-> C++ : CONNECTE");
    Console.WriteLine();

    Console.Write("Chemin de la vidéo MP4 : ");
    string? path = Console.ReadLine();

    if (string.IsNullOrWhiteSpace(path))
    {
        Console.WriteLine("Aucun chemin fourni.");
    }
    else
    {
        path = path.Trim().Trim('"');

        Console.WriteLine();
        Console.WriteLine("Analyse par le moteur JRTE...");
        Console.WriteLine();

        string report = engine.InspectMp4(path);

        Console.WriteLine("===== RAPPORT DU MOTEUR JRTE =====");
        Console.WriteLine(report);

        Console.WriteLine();
        Console.WriteLine("===== EXTRACTION VIDEO =====");

        byte[] sample = engine.ExtractVideoSample(path, 0);

        Console.WriteLine("Extraction : REUSSIE");
        Console.WriteLine($"Echantillon : 0");
        Console.WriteLine($"Taille : {sample.Length} octets");

        Console.WriteLine();
        Console.WriteLine("32 premiers octets :");

        int count = Math.Min(32, sample.Length);

        for (int i = 0; i < count; i++)
        {
            Console.Write($"{sample[i]:X2} ");

            if ((i + 1) % 16 == 0)
                Console.WriteLine();
        }

        Console.WriteLine();
    }
}
catch (DllNotFoundException ex)
{
    Console.WriteLine("ERREUR : DLL introuvable.");
    Console.WriteLine(ex.Message);
}
catch (EntryPointNotFoundException ex)
{
    Console.WriteLine("ERREUR : fonction native introuvable.");
    Console.WriteLine(ex.Message);
}
catch (Exception ex)
{
    Console.WriteLine($"ERREUR : {ex.Message}");
}

Console.WriteLine("================================");
