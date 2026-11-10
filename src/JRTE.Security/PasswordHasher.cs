using System.Security.Cryptography;

namespace JRTE.Security;

public sealed record PasswordHash(
    int Iterations,
    string Salt,
    string Hash
);

public static class PasswordHasher
{
    private const int Iterations = 600_000;
    private const int SaltSize = 16;
    private const int HashSize = 32;
    private const int MinimumLength = 12;

    public static PasswordHash Create(string password)
    {
        ArgumentNullException.ThrowIfNull(password);

        if (password.Length < MinimumLength)
            throw new ArgumentException(
                "Le code doit contenir au moins 12 caractères."
            );

        byte[] salt = RandomNumberGenerator.GetBytes(SaltSize);

        byte[] hash = Rfc2898DeriveBytes.Pbkdf2(
            password,
            salt,
            Iterations,
            HashAlgorithmName.SHA256,
            HashSize
        );

        return new PasswordHash(
            Iterations,
            Convert.ToBase64String(salt),
            Convert.ToBase64String(hash)
        );
    }

    public static bool Verify(
        string password,
        PasswordHash stored
    )
    {
        if (password is null || stored is null)
            return false;

        try
        {
            if (stored.Iterations < 600_000 ||
                stored.Iterations > 2_000_000)
                return false;

            byte[] salt = Convert.FromBase64String(stored.Salt);
            byte[] expected = Convert.FromBase64String(stored.Hash);

            if (salt.Length != SaltSize ||
                expected.Length != HashSize)
                return false;

            byte[] actual = Rfc2898DeriveBytes.Pbkdf2(
                password,
                salt,
                stored.Iterations,
                HashAlgorithmName.SHA256,
                HashSize
            );

            return CryptographicOperations.FixedTimeEquals(
                actual,
                expected
            );
        }
        catch (FormatException)
        {
            return false;
        }
        catch (ArgumentException)
        {
            return false;
        }
    }
}