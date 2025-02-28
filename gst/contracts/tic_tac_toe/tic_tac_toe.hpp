/**
 *  @file
 *  @copyright defined in gst/LICENSE.txt
 */
#include <gstiolib/gstio.hpp>

/**
 *  @defgroup tictactoecontract Tic Tac Toe Contract
 *  @brief Defines the PvP tic tac toe contract example
 *  @ingroup examplecontract
 *  
 *  @details
 *  For the following tic-tac-toe game:
 *  - Each pair of player can have 2 unique game, one where player_1 become host and player_2 become challenger and vice versa
 *  - The game data is stored in the "host" scope and use the "challenger" as the key
 *  
 *  (0,0) coordinate is on the top left corner of the board
 *  @code
 *                 (0,2)
 *  (0,0) -  |  o  |  x      where - = empty cell
 *        -  |  x  |  -            x = move by host
 *  (2,0) x  |  o  |  o            o = move by challenger
 *  @endcode
 * 
 *  Board is represented with number:
 *  - 0 represents empty cell
 *  - 1 represents cell filled by host 
 *  - 2 represents cell filled by challenger
 *  Therefore, assuming x is host, the above board will have the following representation: [0, 2, 1, 0, 1, 0, 1, 2, 2] inside the game object
 * 
 *  In order to deploy this contract:
 *  - Create an account called tic.tac.toe
 *  - Add tic.tac.toe key to your wallet
 *  - Set the contract on the tic.tac.toe account
 * 
 *  How to play the game:
 *  - Create a game using `create` action, with you as the host and other account as the challenger.
 *  - The first move needs to be done by the host, use the `move` action to make a move by specifying which row and column to fill.
 *  - Then ask the challenger to make a move, after that it's back to the host turn again, repeat until the winner is determined.
 *  - If you want to restart the game, use the `restart` action
 *  - If you want to clear the game from the database to save up some space after the game has ended, use the `close` action
 *  @{
 */

class tic_tac_toe : public gstio::contract {
   public:
      tic_tac_toe( account_name self ):contract(self){}
      /**
       * @brief Information related to a game
       * @abi table games i64
       */
      struct game {
         static const uint16_t board_width = 3;
         static const uint16_t board_height = board_width;
         game() { 
            initialize_board(); 
         }
         account_name          challenger;
         account_name          host;
         account_name          turn; // = account name of host/ challenger
         account_name          winner = N(none); // = none/ draw/ name of host/ name of challenger
         std::vector<uint8_t>  board;

         // Initialize board with empty cell
         void initialize_board() {
            board = std::vector<uint8_t>(board_width * board_height, 0);
         }

         // Reset game
         void reset_game() {
            initialize_board();
            turn = host;
            winner = N(none);
         }

         auto primary_key() const { return challenger; }
         GSTLIB_SERIALIZE( game, (challenger)(host)(turn)(winner)(board))
      };

      /**
       * @brief The table definition, used to store existing games and their current state
       */
      typedef gstio::multi_index< N(games), game> games;

      /// @abi action
      /// Create a new game
      void create(const account_name& challenger, const account_name& host);

      /// @abi action
      /// Restart a game
      /// @param by the account who wants to restart the game
      void restart(const account_name& challenger, const account_name& host, const account_name& by);

      /// @abi action
      /// Close an existing game, and remove it from storage
      void close(const account_name& challenger, const account_name& host);

      /// @abi action
      /// Make movement
      /// @param by the account who wants to make the move
      void move(const account_name& challenger, const account_name& host, const account_name& by, const uint16_t& row, const uint16_t& column);
      
};
/// @}
