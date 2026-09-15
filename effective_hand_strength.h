#ifndef EFFECTIVE_HAND_STRENGTH_H
#define EFFECTIVE_HAND_STRENGTH_H

#include "hand_evaluation.h"
#include "helper.h"
#include "raw_equity.h"



// Calculate the hand potential (effective hand strength, hand strength, positive/negative potential)
// potential is the potential to improve (or depreciate) a hand
template <const int LookAhead, const std::size_t streetSize>
inline std::array<std::array<double, 3>, 3> calculate_HP(
    HandInfo p1Info,
    const std::array<uint32_t, 2>& hole,
    const std::array<uint32_t, streetSize>& street,
    const std::vector<uint32_t>& deck
)
{
    // whether a hand is ahead/tie/behind at the given state and after a card (or two) is drawn as a community card
    std::array<std::array<double, 3>, 3> HP {};
    std::vector<std::array<std::array<double, 3>, 3>> threadHP(omp_get_max_threads()); // multithreading

    #pragma omp parallel // parallelisation
    {
        const int threadId = omp_get_thread_num();
        auto& localHP = threadHP[threadId];  // intialise result computed by each thread

        #pragma omp for // parallel for loop
        for (std::size_t i = 0; i < deck.size() - 1; ++i)
        {
            for (std::size_t j = i + 1; j < deck.size(); ++j)
            {
                // enumerate over all possible hands the villain might have
                const std::array<uint32_t, 2> draw {deck[i], deck[j]};
                std::array<uint32_t, 2 + streetSize> currP2Cards {
                    helper::concatenate(draw, street) // cards available current for the villan
                };
                const HandInfo p2Info {determine_score(currP2Cards)};
                const winStatus p1Wins {compare_hands(p1Info, p2Info)}; // see who wins in the current state of the board

                std::array<uint32_t, 2 + streetSize + LookAhead> p1Cards {};
                std::array<uint32_t, 2 + streetSize + LookAhead> p2Cards {};

                for (std::size_t cards = 0; cards < 2; ++cards)
                {
                    p1Cards[cards] = hole[cards];
                    p2Cards[cards] = draw[cards];
                }
                for (std::size_t card = 0; card < streetSize; ++card)
                {
                    p1Cards[2 + card] = street[card];
                    p2Cards[2 + card] = street[card];
                }

                // lambda function takes cards street + drawn to create a new street and adds it to the players available cards
                // it then compute showdown
                auto recordResult = [&](const auto& futureCards)
                {
                    for (int card = 0; card < LookAhead; ++card)
                    {
                        p1Cards[2 + streetSize + card] = futureCards[card];
                        p2Cards[2 + streetSize + card] = futureCards[card];
                    }

                    const HandInfo p1NewInfo {determine_score(p1Cards)};
                    const HandInfo p2NewInfo {determine_score(p2Cards)};
                    const winStatus p1WinsAgain {compare_hands(p1NewInfo, p2NewInfo)};

                    localHP[static_cast<int>(p1Wins)][static_cast<int>(p1WinsAgain)] += 1.0; // increment the table elements of HP
                };

                if constexpr (LookAhead == 1) // draw one card and compare how the hero's hand improve or depreciates (HP1)...
                {
                    for (std::size_t k = 0; k < deck.size(); ++k)
                    {
                        if (k == i || k == j) continue;
                        std::array<uint32_t, 1> cards {deck[k]};
                        recordResult(cards);
                    }
                }
                else // ...or draw two
                {
                    for (std::size_t k = 0; k < deck.size() - 1; ++k)
                    {
                        if (k == i || k == j) continue;
                        for (std::size_t l = k + 1; l < deck.size(); ++l)
                        {
                            if (l == i || l == j) continue;
                            std::array<uint32_t, 2> cards {deck[k], deck[l]};
                            recordResult(cards);
                        }
                    }
                }
            }
        }
    }

    // aggregate parallelisation
    for (const auto& localHP : threadHP)
        for (std::size_t row = 0; row < 3; ++row)
            for (std::size_t column = 0; column < 3; ++column)
                HP[row][column] += localHP[row][column];

    return HP; // return the table
}



template <const std::size_t streetSize>
inline std::array<double, 4> potentialEHS(
    const std::array<uint32_t, 2>& hole,
    const std::array<uint32_t, streetSize>& street,
    const int lookAhead
)
{

    // compute the hand potential table HP
    const std::array<uint32_t, 2 + streetSize> cardsInPlay {
        helper::concatenate(hole, street)
    };
    const std::vector<uint32_t> deck {construct_deck(cardsInPlay)};
    const HandInfo p1Info {determine_score(cardsInPlay)};
    std::array<std::array<double, 3>,3> HP {};
    if (lookAhead == 1)
        HP = calculate_HP<1>(p1Info, hole, street, deck);
    else if (lookAhead == 2)
        HP = calculate_HP<2>(p1Info, hole, street, deck);

    // compute the total number of games played starting at ahead/tied/behind originally
    const std::array<double, 3> totalHP {
        HP[0][0] + HP[0][1] + HP[0][2],
        HP[1][0] + HP[1][1] + HP[1][2],
        HP[2][0] + HP[2][1] + HP[2][2]
    };

    const double total = totalHP[0] + totalHP[1] + totalHP[2]; // total number of possible games played
    if (total == 0.0) return {0.0, 0.0, 0.0, 0.0};

    double HS = (totalHP[1] + 0.5 * totalHP[2]) / total; // calculate hand strength
    // Note: flop + HP2 gives HS = raw equity (assuming villain has uniform range)

    // weighted games starting in a positive (ahead & tied) and a negative (behind & tied)
    const double positiveDenominator = totalHP[0] + 0.5 * totalHP[2];
    const double negativeDenominator = totalHP[1] + 0.5 * totalHP[2];

    double PPot = (positiveDenominator > 0.0) // avoid divide by zero error
        ? (HP[0][1] + 0.5 * HP[0][2] + 0.5 * HP[2][1]) / positiveDenominator // positive potential moving from: behind -> ahead, tied -> ahead, behind -> draw
        : 0.0;

    double NPot = (negativeDenominator > 0.0) 
        ? (HP[1][0] + 0.5 * HP[1][2] + 0.5 * HP[2][0]) / negativeDenominator // negative potential moving from: ahead -> draw, tied -> behind, ahead -> behind
        : 0.0;

    double EHS = HS * (1.0 - NPot) + (1.0 - HS) * PPot; // computes effective hand strength formula

    return {EHS, HS, PPot, NPot};
}
#endif