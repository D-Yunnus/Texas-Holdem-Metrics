#ifndef EQUITY_HISTOGRAM_H
#define EQUITY_HISTOGRAM_H

#include "hand_evaluation.h"
#include "raw_equity.h"
#include "helper.h"
#include "effective_hand_strength.h"



// Generates the equity histogram (probability density) of a certain gamestate
// the equity histogram is the probability (given the initial gamestate) of drawing a card
// and the equity of the potential gamestate is within a certain equity bin of the histogram
// determine the structure of potential improvement (or depreciation)
template <const std::size_t binNums, const std::size_t streetSize>
inline std::array<double, binNums> equity_histogram(
    const std::array<uint32_t, 2>& holes,
    const std::array<uint32_t, streetSize>& street // (TODO: assert 3 <= streetSize < 5)
)
{
    std::array<double, binNums> bins {}; // histogram bins
    const double binWidth = 1.0 / static_cast<double>(binNums); // size of bin intervals of histogram

    const std::array<uint32_t, 2 + streetSize> cardsInPlay {
        helper::concatenate(holes, street)
    };
    const std::vector<uint32_t> deck = construct_deck(cardsInPlay);

    for (std::size_t i = 0; i < deck.size(); ++i) // iterate over drawing each of the remaining cards in the deck as the next comunity cards
    {
        // draw a card into the community cards and compute equity of the new game state
        const std::array<uint32_t, 1 + streetSize> newStreet {
            helper::concatenate(street, std::array<uint32_t, 1> {deck[i]})
        };
        const double equity = rand_raw_equity(holes, newStreet);

        // check which bin the equity falls into and increment it appropriately
        for (std::size_t j = 1; j < binNums; ++j)
        {
            const double lowerBound = static_cast<double>(j - 1) * binWidth;
            const double upperBound = static_cast<double>(j) * binWidth;

            if (equity >= lowerBound && equity < upperBound)
            {
                bins[j - 1] += 1.0 / deck.size();
            }
        }
        if (equity >= static_cast<double>(binNums - 1) * binWidth)
        {
            bins[binNums - 1] += 1.0 / deck.size();
        }
    }
    return bins; // returns histogram values as a vector
}



// Given two equity histograms find the minimum number of units from each bin to move to another bin so that histogram 1 -> histogram 2
// the minimum number of units is the Earth Mover's distance (Wasserstein metric) and quantifies how close two equity histograms are
// close equity histograms implies the gamestates of those histograms can be considered similar (useful for bucketing states)
template <const std::size_t binNums>
inline double compute_EDM(
    const std::array<double, binNums>& histogram1,
    const std::array<double, binNums>& histogram2
)
{
    const double binWidth = 1.0 / static_cast<double>(binNums);
    double cumulativeDifference {0.0};
    double EDM {0.0};

    // standard procedural EDM calculator for 1D ordered sets
    for (std::size_t i = 0; i < binNums; ++i)
    {
        cumulativeDifference += histogram1[i] - histogram2[i];
        EDM += std::abs(cumulativeDifference);
    }

    return binWidth * EDM;
}
#endif