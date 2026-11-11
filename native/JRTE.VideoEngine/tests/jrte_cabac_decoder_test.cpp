#include "jrte_cabac_decoder.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, const std::string& name)
{
    ++checks;

    if (condition) {
        std::cout << "[PASS] " << name << '\n';
    } else {
        ++failures;
        std::cerr << "[FAIL] " << name << '\n';
    }
}

void test_initialization()
{
    jrte::CabacDecoder decoder;
    const uint8_t data[] = {0x00, 0x00};

    check(decoder.initialize(data, sizeof(data)),
          "Initialisation avec un flux valide");

    check(decoder.range() == 510,
          "Range initial egal a 510");

    check(decoder.offset() == 0,
          "Offset initial egal a zero");

    check(decoder.good(),
          "Etat du decodeur valide apres initialisation");
}

void test_invalid_initialization()
{
    jrte::CabacDecoder decoder;

    check(!decoder.initialize(nullptr, 2),
          "Refus d'un pointeur nul");

    check(!decoder.good(),
          "Etat invalide apres pointeur nul");

    const uint8_t empty_data[] = {0x00};

    check(!decoder.initialize(empty_data, 0),
          "Refus d'un flux vide");

    const uint8_t short_data[] = {0x00};

    check(!decoder.initialize(short_data, sizeof(short_data)),
          "Refus d'un flux trop court pour 9 bits");
}

void test_bypass()
{
    jrte::CabacDecoder decoder;
    const uint8_t data[] = {0x00, 0x00, 0x00};

    check(decoder.initialize(data, sizeof(data)),
          "Initialisation pour le test bypass");

    uint32_t bin = 99;

    check(decoder.decode_bypass(bin),
          "Decodage bypass d'un bit zero");

    check(bin == 0,
          "Resultat bypass zero");

    check(decoder.good(),
          "Decodeur toujours valide apres bypass");
}

void test_bypass_transition()
{
    jrte::CabacDecoder decoder;

    // Les 9 premiers bits valent 255.
    // Le bit suivant vaut zero.
    const uint8_t data[] = {0x7F, 0x80, 0x00};

    check(decoder.initialize(data, sizeof(data)),
          "Initialisation pour transition bypass");

    check(decoder.offset() == 255,
          "Offset initial de 255");

    uint32_t bin = 99;

    check(decoder.decode_bypass(bin),
          "Execution du bypass avec depassement");

    check(bin == 1,
          "Transition bypass vers le bin 1");

    check(decoder.offset() == 0,
          "Offset corrige apres soustraction du range");
}

void test_terminate()
{
    jrte::CabacDecoder decoder;
    const uint8_t data[] = {0x00, 0x00, 0x00};

    check(decoder.initialize(data, sizeof(data)),
          "Initialisation pour terminate");

    uint32_t bin = 99;

    check(decoder.decode_terminate(bin),
          "Decodage terminate");

    check(bin == 0,
          "Terminate retourne zero lorsque offset est inferieur au range");

    check(decoder.range() == 508,
          "Range reduit de deux apres terminate zero");

    check(decoder.good(),
          "Decodeur valide apres terminate zero");
}

} // namespace

int main()
{
    std::cout << "JRTE CABAC Decoder Tests\n";
    std::cout << "-----------------------\n";

    test_initialization();
    test_invalid_initialization();
    test_bypass();
    test_bypass_transition();
    test_terminate();

    std::cout << "\nResultat : "
              << (checks - failures) << "/" << checks
              << " tests reussis.\n";

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
