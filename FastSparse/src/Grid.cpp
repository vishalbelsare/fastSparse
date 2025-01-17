#include "Grid.h"

// Assumes PG.P.Specs have been already set
template <class T>
Grid<T>::Grid(const T& X, const arma::vec& y, const GridParams<T>& PGi) {
    PG = PGi;
    // std::cout << "Grid.cpp i'm in line 7\n";
    
    if (!PG.P.Specs.Exponential) { // if the loss is not exponential
        std::tie(BetaMultiplier, meanX, meany, scaley) = Normalize(X, 
                y, Xscaled, yscaled, !PG.P.Specs.Classification, PG.intercept);
    } else { // if the loss is exponential
        Xscaled = 2*X-1;
        meanX.set_size(X.n_cols);
        meanX.fill(0.5);
        BetaMultiplier.set_size(X.n_cols);
        BetaMultiplier.fill(2.0);
        meany = 0;
        scaley = 1;
        yscaled = y;
    }
    // Must rescale bounds by BetaMultiplier in order for final result to conform to bounds
    if (PG.P.withBounds){
        PG.P.Lows /= BetaMultiplier;
        PG.P.Highs /= BetaMultiplier;   
    }
}

template <class T>
void Grid<T>::Fit() {
    
    std::vector<std::vector<std::unique_ptr<FitResult<T>>>> G;
    
    if (PG.P.Specs.L0) {
        // L0 is only used for linear regression
        // for other losses, a small L2 is added in fit.R so that the code runs the else statement below
        // std::cout << "Grid.cpp i'm in line 37\n";
        G.push_back(std::move(Grid1D<T>(Xscaled, yscaled, PG).Fit()));
        Lambda12.push_back(0);
    } else {
        // std::cout << "Grid.cpp i'm in line 41\n";
        G = std::move(Grid2D<T>(Xscaled, yscaled, PG).Fit());
    }

    // std::cout << "Grid.cpp i'm in line 45\n";
    
    Lambda0 = std::vector< std::vector<double> >(G.size());
    NnzCount = std::vector< std::vector<std::size_t> >(G.size());
    Solutions = std::vector< std::vector<arma::sp_mat> >(G.size());
    Intercepts = std::vector< std::vector<double> >(G.size());
    Converged = std::vector< std::vector<bool> >(G.size());
    Objectives = std::vector< std::vector<double> >(G.size());

    for (std::size_t i=0; i<G.size(); ++i) {
        if (PG.P.Specs.L0L1){ 
            Lambda12.push_back(G[i][0]->ModelParams[1]); 
            // std::cout << "Grid.cpp i'm in line 41\n";
        } else if (PG.P.Specs.L0L2) { 
            Lambda12.push_back(G[i][0]->ModelParams[2]); 
            // std::cout << "Grid.cpp i'm in line 44\n";
        }
        
        for (auto &g : G[i]) {
            Lambda0[i].push_back(g->ModelParams[0]);
            
            NnzCount[i].push_back(n_nonzero(g->B));
            
            if (g->IterNum != PG.P.MaxIters){
                Converged[i].push_back(true);
            } else {
                Converged[i].push_back(false);
            }
            
            Objectives[i].push_back(g->Objective);

            beta_vector B_unscaled;
            double b0;
            
            std::tie(B_unscaled, b0) = DeNormalize(g->B, BetaMultiplier, meanX, meany);
            Solutions[i].push_back(arma::sp_mat(B_unscaled));
            /* scaley is 1 for classification problems.
             *  g->intercept is 0 unless specifically optimized for in:
             *       classification
             *       sparse regression and intercept = true
             */
            Intercepts[i].push_back(scaley*g->b0 + b0);
        }
    }
}

template class Grid<arma::mat>;
template class Grid<arma::sp_mat>;
