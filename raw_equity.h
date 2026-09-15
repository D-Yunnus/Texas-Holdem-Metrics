#ifndef RAW_EQUITY_H
#define RAW_EQUITY_H

#include "hand_evaluation.h"
#include "helper.h"




// Gives a deck of face-down cards (excluding cards already in play) to draw from
template <const std::size_t cardCount>
inline constexpr std::vector<uint32_t> construct_deck(
    const std::array<uint32_t, cardCount>& cards_in_play
)
{
    std::vector<uint32_t> deck;
    deck.reserve(52 - cardCount);
    for (int suit {SPADES}; suit <= HEARTS; ++suit)
    {
        for (int rank {TWO}; rank <= ACE; ++rank)
        {
            uint32_t card {bitwise_card(static_cast<Ranks>(rank), static_cast<Suits>(suit))};
            if (std::find(cards_in_play.begin(), cards_in_play.end(), card) != cards_in_play.end())
            {
                continue;
            }
            deck.push_back(card);
        }
    }
    return deck;
}



// Draws a random card from a deck of face-down cards
inline uint32_t draw_card(
    std::vector<uint32_t>& deck,
    std::mt19937& randGen)
{
    std::uniform_int_distribution<int> dist(0, deck.size() - 1);
    const int &randomIndex {dist(randGen)};
    std::swap(deck[randomIndex], deck.back()); // partial-1 Fisher-Yates shuffle
    const uint32_t draw = deck.back();
    deck.pop_back(); // removes card from deck

    return draw;
}


// Computes raw equity using a monte-carlo method
template <const std::size_t p2HoleSize, const std::size_t streetSize>
inline double raw_equity_monte_carlo(
    const std::array<uint32_t, 2>& p1Holes,
    const std::array<uint32_t, p2HoleSize>& p2KnownCards,
    const std::array<uint32_t, streetSize>& street,
    std::mt19937& randGen,
    const int numIter
)
{
    const std::array<uint32_t, 2 + p2HoleSize + streetSize> cardsInPlay {
        helper::concatenate(p1Holes, helper::concatenate(p2KnownCards, street))};
    const std::vector<uint32_t> deck {construct_deck(cardsInPlay)};

    double equity {0};
    const std::mt19937::result_type seed {randGen()};

    // Monte carlo parallisation using OpenMP
    #pragma omp parallel for reduction(+:equity)
    for (int iter = 0; iter < numIter; ++iter)
    {
        std::vector<uint32_t> localDeck {deck};
        std::mt19937 localGen {seed + iter};

        std::array<uint32_t, 2> p2Holes {};

        // randomly samples remaining cards to complete the villains hole card (if needed)
        if (p2HoleSize <= 2)
        {
            std::array<uint32_t, 2 - p2HoleSize> draw {};
            for (std::size_t i = 0; i < 2 - p2HoleSize; ++i)
                draw[i] = draw_card(localDeck, localGen);
            p2Holes = helper::concatenate(p2KnownCards, draw);
        }
        
        // randomly samples remaining cards to complete the river (if needed)
        std::array<uint32_t, 5> riverCards {};
        if (streetSize <= 5)
        {
            std::array<uint32_t, 5 - streetSize> communityCards {};
            for (std::size_t i = 0; i < 5 - streetSize; ++i)
                communityCards[i] = draw_card(localDeck, localGen);
            riverCards = helper::concatenate(street, communityCards);
        }

        // cards available to each player to create their five card best hand
        const std::array<uint32_t, 7> p1Cards = helper::concatenate(p1Holes, riverCards);
        const std::array<uint32_t, 7> p2Cards = helper::concatenate(p2Holes, riverCards);
        const HandInfo p1Info {determine_score(p1Cards)};
        const HandInfo p2Info {determine_score(p2Cards)};
        const winStatus result {compare_hands(p1Info, p2Info)}; // showdown result

        if (result == WIN) equity += 1; // if the hero wins
        else if (result == DRAW) equity += 0.5; // if the hero and villain draw
    }

    return equity /= numIter; // turn into a percentage
}



// Enumerate over all remaining card combinations to complete villains hand and river to calculate equity
template <const std::size_t streetSize> // (TODO: assert 3 <= streetSize <=5)
inline double p2_unknown_equity(
    const std::array<uint32_t, 2>& p1Holes,
    const std::array<uint32_t, streetSize>& street
)
{
    std::array<double, 3> wins {}; // wins[0] = number of losses, wins[1] = number of wins, wins[2] = number of draws
    std::vector<std::array<double, 3>> threadWins(omp_get_max_threads()); // allocate a batch of computations for each available thread

    const std::array<uint32_t, streetSize + 2> cardsInPlay {
        helper::concatenate(p1Holes, street)
    };
    const std::vector<uint32_t> deck {construct_deck(cardsInPlay)};

    #pragma omp parallel // parallelisation
    {
        const int threadId = omp_get_thread_num();
        auto& localWins = threadWins[threadId]; // intialise result computed by each thread

        #pragma omp for // parallel for loop
        for (std::size_t i = 0; i < deck.size() - 1; ++i)
        {
            for (std::size_t j = i + 1; j < deck.size(); ++j)
            {
                const std::array<uint32_t, 2> p2Holes {deck[i], deck[j]};

                std::array<uint32_t, 7> p1Cards {}; // cards available for the heros use
                std::array<uint32_t, 7> p2Cards {}; // cards available for the villains use
                for (std::size_t cards = 0; cards < 2; ++cards)
                {
                    p1Cards[cards] = p1Holes[cards];
                    p2Cards[cards] = p2Holes[cards];
                }
                for (std::size_t cards = 0; cards < streetSize; ++cards)
                {
                    p1Cards[cards + 2] = street[cards];
                    p2Cards[cards + 2] = street[cards];
                }

                // lambda function takes in the number of cards drawns adds it to the hero and villains available cards and determines who wins
                auto recordResult = [&](const auto& nextCards)
                {
                    for (std::size_t cards = 0; cards < 5 - streetSize; ++cards)
                    {
                        p1Cards[cards + 2 + streetSize] = nextCards[cards];
                        p2Cards[cards + 2 + streetSize] = nextCards[cards];
                    }

                    const winStatus p1Wins {compare_hands(determine_score(p1Cards), determine_score(p2Cards))};
                    localWins[static_cast<int>(p1Wins)] += 1.0; // increment whichever winStatus it is
                };

                if constexpr (streetSize == 3) // flop (cycle through all possible 2 cards drawn from the deck)
                {
                    for (std::size_t k = 0; k < deck.size() - 1; ++k)
                    {
                        if (k == i || k == j) continue;
                        for (std::size_t l = k + 1; l < deck.size(); ++l)
                        {
                            if (l == i || l == j) continue;
                            const std::array<uint32_t, 2> cards {deck[k], deck[l]};
                            recordResult(cards);
                        }
                    }
                }

                if constexpr (streetSize == 4) // turn (cycle through all possible 1 card drawn from the deck)
                {
                    for (std::size_t k = 0; k < deck.size(); ++k)
                    {
                        if (k == i || k == j) continue;
                        const std::array<uint32_t, 1> card {deck[k]};
                        recordResult(card);
                    }
                }

                if constexpr (streetSize == 5) // river (no need to draw cards)
                {
                    const std::array<uint32_t, 0> card {};
                    recordResult(card);
                }

            }

        }
    }

    // aggregate parallelisation
    for (const auto& localWins : threadWins)
        for (int i = 0; i < 3; ++i)
            wins[i] += localWins[i];

    return (wins[1] + 0.5*wins[2]) / (wins[0] + wins[1] + wins[2]); // normalise over total number of possible games
}



// Calculate raw equity of a given hero hole and street (villain hand is unknown and random until showdown)
template <const std::size_t streetSize>
inline double rand_raw_equity(
    const std::array<uint32_t, 2>& p1Holes,
    const std::array<uint32_t, streetSize>& street
)
{
    // use Monte Carlo for no street (quicker in this case)
    if (static_cast<int>(streetSize) == 0)
    {
        std::mt19937 randGen (42);
        const std::array<uint32_t, 0> p2Holes {};
        return raw_equity_monte_carlo(p1Holes, p2Holes, street, randGen, 1000000);
    }
    
    // otherwise enumerate over all possible games
    return p2_unknown_equity(p1Holes, street);
}
#endif