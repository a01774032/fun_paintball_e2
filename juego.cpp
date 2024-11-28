#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <random>
#include <ctime>
#include <algorithm>
#include <map>
#include <iomanip>
#include <termios.h>
#include <unistd.h>
#include <thread>
#include <chrono>

#include <SDL.h>
#include <SDL_mixer.h>

using namespace std;

// Direction enum for clarity
enum Direction { UP = 1, LEFT, DOWN, RIGHT };

// Cell class declaration (keep this at the top)
class Cell {
private:
    vector<class Player*> players;
public:
    void addPlayer(class Player* player);
    void removePlayer(class Player* player);
    const vector<class Player*>& getPlayers() const;
};

// Player base class
class Player {
protected:
    int id;
    char team; // 'R' for Red, 'B' for Blue
    int x, y; // Position on the board
    int hitsToExtremities;
    bool eliminated;
    bool fast; // True if fast, false if slow
    bool expert; // True if expert, false if novice
    int maxMovement;
    int range;
    double torsoHitChance;
    double extremityHitChance;
    double headHitChance;
    bool shooterEliminated; // True if the shooter is eliminated due to headshot
    string eliminationReason;
    int startX, startY; // Add starting position
    bool moved; // Renamed from hasMoved to moved
public:
    Player(int id, char team, bool fast, bool expert);
    int getId() const;
    char getTeam() const;
    int getX() const;
    int getY() const;
    void setPosition(int x, int y);
    bool isEliminated() const;
    bool isShooterEliminated() const;
    void setEliminated(bool status, const string& reason = "");
    std::string move(int direction, int squares, vector<vector<Cell>>& board, mt19937& rng);
    pair<bool, string> attack(int direction, int squares, vector<vector<Cell>>& board, mt19937& rng);
    bool isFast() const { return fast; }
    bool isExpert() const { return expert; }
    int getMaxMovement() const { return maxMovement; }
    int getHitsToExtremities() const {
        return hitsToExtremities;
    }
    int getAttackRange() const { return range; }
    string getEmojiRepresentation() const;
    string getEliminationReason() const;
    void setStartPosition(int x, int y);
    std::pair<int, int> getStartPosition() const;
    bool hasMoved() const;
    void resetMoved();
};

// Cell implementation (moved here, after Player class)
void Cell::addPlayer(Player* player) {
    players.push_back(player);
    // Sort players by ID after adding new player
    sort(players.begin(), players.end(), 
         [](Player* a, Player* b) { return a->getId() < b->getId(); });
}

void Cell::removePlayer(Player* player) {
    players.erase(remove(players.begin(), players.end(), player), players.end());
}

const vector<Player*>& Cell::getPlayers() const {
    return players;
}

Player::Player(int id, char team, bool fast, bool expert)
    : id(id), team(team), fast(fast), expert(expert), hitsToExtremities(0),
      eliminated(false), shooterEliminated(false), moved(false)
{
    x = y = -1; // Initial position not set
    
    // Set movement capacity based on speed
    maxMovement = fast ? 2 : 1;

    // Set hit probabilities based on expertise
    if (expert) {
        torsoHitChance = 0.6;
        extremityHitChance = 0.85;
        headHitChance = 0.05;
        range = 2; // Expert can attack up to 2 squares
    } else {
        torsoHitChance = 0.1;
        extremityHitChance = 0.5;
        headHitChance = 0.25;
        range = 1; // Novice can only attack 1 square
    }
}

int Player::getId() const { return id; }
char Player::getTeam() const { return team; }
int Player::getX() const { return x; }
int Player::getY() const { return y; }
void Player::setPosition(int x, int y) { this->x = x; this->y = y; }
bool Player::isEliminated() const { return eliminated; }
bool Player::isShooterEliminated() const { return shooterEliminated; }
void Player::setEliminated(bool status, const string& reason) {
    eliminated = status;
    if (status) {
        eliminationReason = reason;
    }
}

std::string Player::move(int direction, int squares, vector<vector<Cell>>& board, mt19937& rng) {
    moved = true; // Updated variable name
    std::stringstream actionStream;

    if (squares == -1) { // Random movement for automatic mode
        if (fast) {
            uniform_real_distribution<> dis(0, 1);
            squares = (dis(rng) <= 0.5) ? 2 : 1; // 50% chance for moving 2 squares
        } else {
            squares = 1; // Slow players can only move 1 square
        }
    }

    // Validate movement capacity
    if (squares > maxMovement) {
        actionStream << "Cannot move " << squares << " squares. Maximum movement is " << maxMovement << ".";
        return actionStream.str();
    }

    int dx = 0, dy = 0;
    switch (direction) {
        case UP: dy = -1; break;
        case DOWN: dy = 1; break;
        case LEFT: dx = -1; break;
        case RIGHT: dx = 1; break;
        default: 
            actionStream << "Invalid direction.";
            return actionStream.str();
    }

    for (int i = 1; i <= squares; ++i) {
        int nx = x + dx * i;
        int ny = y + dy * i;
        if (nx < 0 || ny < 0 || ny >= board.size() || nx >= board[0].size()) {
            actionStream << "Movement would go out of bounds.";
            return actionStream.str();
        }
        Cell& cell = board[ny][nx];
        const vector<Player*>& playersInCell = cell.getPlayers();

        // Check for opponent players in any cell along the path
        for (Player* p : playersInCell) {
            if (p->getTeam() != team) {
                actionStream << "Cannot move into or through a cell occupied by opponent players.";
                return actionStream.str();
            }
        }

        // Check max 4 players per cell only for the destination cell
        if (i == squares && playersInCell.size() >= 4) {
            actionStream << "Destination cell is full (max 4 players per cell).";
            return actionStream.str();
        }
    }

    // Move the player
    board[y][x].removePlayer(this);
    x += dx * squares;
    y += dy * squares;

    // Attempt to add player to the new cell
    board[y][x].addPlayer(this);

    actionStream << "Player " << id << " moved to (" << x << ", " << y << ").";
    return actionStream.str();
}

pair<bool, string> Player::attack(int direction, int squares, vector<vector<Cell>>& board, mt19937& rng) {
    moved = true; // Updated variable name
    // 1. Validate direction (orthogonal attacks only)
    int dx = 0, dy = 0;
    switch (direction) {
        case UP: dy = -1; break;
        case DOWN: dy = 1; break;
        case LEFT: dx = -1; break;
        case RIGHT: dx = 1; break;
        default: 
            cout << "Invalid attack direction. Must be UP(1), LEFT(2), DOWN(3), or RIGHT(4).\n";
            return {false, "Invalid attack direction. Must be UP(1), LEFT(2), DOWN(3), or RIGHT(4)."};
    }

    // 2. Determine and validate attack range
    int attackRange;
    if (squares == -1) { // Random range for automatic mode
        if (expert) {
            uniform_real_distribution<> dis(0, 1);
            attackRange = (dis(rng) <= 0.75) ? 1 : 2; // 75% chance for 1 square, 25% for 2
        } else {
            attackRange = 1; // Novice can only attack 1 square
        }
        squares = attackRange;
    } else {
        // Validate manual range input
        int maxRange = static_cast<bool>(expert) ? 2 : 1; // Explicitly cast 'expert' to bool
        if (squares > maxRange || squares < 1) {
            cout << "Invalid attack range. ";
            cout << (expert ? "Expert players can attack 1-2 squares." : "Novice players can only attack 1 square.");
            cout << "\n";
            return {false, std::string("Invalid attack range. ") + (expert ? "Expert players can attack 1-2 squares." : "Novice players can only attack 1 square.")};
        }
    }

    // Calculate target position
    int targetX = x + (dx * squares);
    int targetY = y + (dy * squares);
    
    // Check if target is in bounds
    if (targetX < 0 || targetY < 0 || targetY >= board.size() || targetX >= board[0].size()) {
        cout << "Attack target is out of bounds.\n";
        return {false, "Attack target is out of bounds."};
    }

    // 3. Check line of sight (no obstacles between attacker and target)
    for (int i = 1; i <= squares; i++) {
        int checkX = x + (dx * i);
        int checkY = y + (dy * i);
        Cell& checkCell = board[checkY][checkX];
        
        // If we find any players before the target square, attack is blocked
        if (i < squares && !checkCell.getPlayers().empty()) {
            cout << "Line of sight blocked by players in intermediate squares.\n";
            return {false, "Line of sight blocked by players in intermediate squares."};
        }
        
        // When we reach target square, check for valid targets
        if (i == squares) {
            const vector<Player*>& playersInCell = checkCell.getPlayers();
            
            // Check if there are any valid targets (non-eliminated enemies)
            bool hasValidTarget = false;
            for (Player* target : playersInCell) {
                if (target->getTeam() != team && !target->isEliminated()) {
                    hasValidTarget = true;
                    break;
                }
            }

            if (!hasValidTarget) {
                cout << "No valid targets in range.\n";
                return {false, "No valid targets in range."};
            }

            // 4. Select first valid target and perform attack
            for (Player* target : playersInCell) {
                if (target->getTeam() != team && !target->isEliminated()) {
                    uniform_real_distribution<> dis(0, 1);
                    double hitRoll = dis(rng);

                    if (hitRoll < headHitChance) {
                        eliminated = true;
                        shooterEliminated = true;
                        eliminationReason = "Headshot penalty";
                        return {true, "Player " + to_string(id) + " hit opponent's head and is eliminated due to rule violation!"};
                    } else if (hitRoll < headHitChance + torsoHitChance) {
                        target->setEliminated(true, "Hit in torso");
                        return {true, "Player " + to_string(id) + " hit opponent player " + to_string(target->getId())
                                     + "'s torso! Player " + to_string(target->getId()) + " is eliminated!"};
                    } else if (hitRoll < headHitChance + torsoHitChance + extremityHitChance) {
                        target->hitsToExtremities++;
                        string result = "Player " + to_string(id) + " hit opponent player " + to_string(target->getId())
                                        + "'s extremity! (" + to_string(target->hitsToExtremities) + "/3 hits)";
                        if (target->hitsToExtremities >= 3) {
                            target->setEliminated(true, "3 extremity hits");
                            result += " Player " + to_string(target->getId()) + " received 3 hits to extremities and is eliminated!";
                        }
                        return {true, result};
                    }

                    // Miss
                    return {false, "Player " + to_string(id) + " missed the shot."};
                }
            }
        }
    }

    return {false, "No valid targets in range."};
}

string Player::getEmojiRepresentation() const {
    string expertEmoji = expert ? "🎯" : "🔰";
    string speedEmoji = fast ? "🏃" : "🐢";
    string hits = "(" + to_string(hitsToExtremities) + ")";
    string idStr = to_string(id);
    return expertEmoji + speedEmoji + hits + "[" + idStr + "]";
}

string Player::getEliminationReason() const {
    return eliminationReason;
}

void Player::setStartPosition(int x, int y) {
    startX = x;
    startY = y;
}

std::pair<int, int> Player::getStartPosition() const {
    return {startX, startY};
}

bool Player::hasMoved() const {
    return moved;
}

void Player::resetMoved() {
    moved = false;
}

// Game class
class Game {
private:
    int boardSize;
    int numPlayersPerTeam;
    vector<vector<Cell>> board;
    vector<Player*> redTeam;
    vector<Player*> blueTeam;
    mt19937 rng;
    char userTeam;
    int turns;
    bool gameEnded;
    string winner;
    pair<int, int> redFlag;
    pair<int, int> blueFlag;
    map<int, Player*> playerMap; // Map player IDs to player objects
    vector<pair<char, string>> actionHistory;
    Mix_Music* bgm;
    Mix_Chunk* jumpSound;
    Mix_Chunk* gameoverSound;
    std::thread escapeThread;
    bool redTeamMoved;
    bool blueTeamMoved;
    int playerIDCounter;
public:
    Game();
    void initialize();
    void displayBoard();
    void play();
    void userTurn();
    void programTurn();
    bool checkEndConditions();
    ~Game();
    void displayBoardWithCursor(int cursorX, int cursorY, int playerIndex);
    void displaySplashScreen();
    void displayGameOverScreen();
    void animateText(const string& text);
    void displayAttackAnimation(int fromX, int fromY, int toX, int toY);
    void playMusic(const std::string& musicFilePath);
    void stopMusic();
    int getVisibleLength(const string& s) const;
    int getDisplayWidth(const string& s) const;
    void resetPlayersMovedFlag();
};

Game::Game() : redTeamMoved(false), blueTeamMoved(false), playerIDCounter(0) {
    rng.seed(static_cast<unsigned int>(time(0)));
    turns = 0;
    gameEnded = false;

    // Randomly choose starting team
    uniform_int_distribution<> startTeamDis(0, 1);
    userTeam = startTeamDis(rng) == 0 ? 'R' : 'B';

    // Randomly assign flag colors
    uniform_int_distribution<> flagColorDis(0, 1);
    if (flagColorDis(rng) == 0) {
        redFlag = make_pair(0, 0);
        blueFlag = make_pair(boardSize - 1, boardSize - 1);
    } else {
        redFlag = make_pair(boardSize - 1, boardSize - 1);
        blueFlag = make_pair(0, 0);
    }

    // Initialize SDL2
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return;
    }

    // Initialize SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << std::endl;
        SDL_Quit();
        return;
    }

    // Load background music
    bgm = Mix_LoadMUS("music/juego.mp3");
    if (!bgm) {
        std::cerr << "Failed to load background music! SDL_mixer Error: " << Mix_GetError() << std::endl;
    }

    // **Load sound effects**
    jumpSound = Mix_LoadWAV("music/jump.mp3");
    if (!jumpSound) {
        std::cerr << "Failed to load jump sound effect! SDL_mixer Error: " << Mix_GetError() << std::endl;
    }

    gameoverSound = Mix_LoadWAV("music/gameover.mp3");
    if (!gameoverSound) {
        std::cerr << "Failed to load game over sound effect! SDL_mixer Error: " << Mix_GetError() << std::endl;
    }
}

 // Start of Selection
void Game::initialize() {
    int numRows, numCols;
    string input;

    cout << "Enter number of rows for the board: ";
    getline(cin, input);
    numRows = stoi(input);
    while (numRows <= 0) {
        cout << "Invalid number of rows. Enter again: ";
        getline(cin, input);
        numRows = stoi(input);
    }

    cout << "Enter number of columns for the board: ";
    getline(cin, input);
    numCols = stoi(input);
    while (numCols <= 0) {
        cout << "Invalid number of columns. Enter again: ";
        getline(cin, input);
        numCols = stoi(input);
    }

    // Initialize board with specified rows and columns
    board.resize(numRows, vector<Cell>(numCols));

    // Set boardSize based on the input dimensions
    boardSize = numRows * numCols;

    // Randomly assign flag positions
    uniform_int_distribution<> flagColorDis(0, 1);
    if (flagColorDis(rng) == 0) {
        redFlag = make_pair(0, 0);
        blueFlag = make_pair(numCols - 1, numRows - 1);
    } else {
        redFlag = make_pair(numCols - 1, numRows - 1);
        blueFlag = make_pair(0, 0);
    }

    // Ask user for the number of players per team
    cout << "Enter number of players per team: ";
    getline(cin, input);
    numPlayersPerTeam = stoi(input);
    while (numPlayersPerTeam <= 0) {
        cout << "Invalid number of players. Enter again: ";
        getline(cin, input);
        numPlayersPerTeam = stoi(input);
    }

    // Initialize players for each team
    uniform_real_distribution<> playerTypeDis(0, 1);

    // Function to place players starting from a base cell
    auto placePlayers = [&](vector<Player*>& teamPlayers, char team, int startX, int startY) {
        int x = startX;
        int y = startY;
        int playersAdded = 0;

        while (playersAdded < numPlayersPerTeam) {
            Cell& cell = board[y][x];

            // Only add one player per cell during initialization
            if (playersAdded < numPlayersPerTeam) {
                // Determine player type based on probabilities
                double randomValue = playerTypeDis(rng);
                bool fast = false, expert = false;

                if (randomValue < 0.15) {
                    fast = true; expert = true; // Fast Expert (ER)
                } else if (randomValue < 0.40) {
                    fast = false; expert = true; // Slow Expert (EL)
                } else if (randomValue < 0.90) {
                    fast = true; expert = false; // Fast Novice (NR)
                } else {
                    fast = false; expert = false; // Slow Rookie (NL)
                }

                Player* player = new Player(playerIDCounter, team, fast, expert);
                player->setPosition(x, y);
                player->setStartPosition(x, y); // Set the starting position
                teamPlayers.push_back(player);
                playerMap[player->getId()] = player;
                cell.addPlayer(player);

                playerIDCounter++; // Increment the playerIDCounter
                playersAdded++;
            }

            // Move to the next cell in the row or column
            if (startX == 0) x++;
            else x--;

            if (x < 0 || x >= numCols) {
                x = startX;
                if (startY == 0) y++;
                else y--;
                if (y < 0 || y >= numRows) break;
            }
        }
    };

    // Place Red Team players starting from redFlag position
    placePlayers(redTeam, 'R', redFlag.first, redFlag.second);

    // Place Blue Team players starting from blueFlag position
    placePlayers(blueTeam, 'B', blueFlag.first, blueFlag.second);
}

string getCurrentTime() {
    time_t now = time(0);
    tm* localtm = localtime(&now);
    char buffer[9];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", localtm);
    return string(buffer);
}

void Game::play() {
    // Define color codes
    const string RED = "\033[31m";
    const string BLUE = "\033[34m";
    const string RESET = "\033[0m";

    displaySplashScreen(); // Display splash screen

    initialize();

    // Start playing music after initialization
    playMusic("music/juego.mp3");

    // Randomly decide which team starts
    uniform_int_distribution<> startTeamDis(0, 1);
    char currentTeam = startTeamDis(rng) == 0 ? userTeam : (userTeam == 'R' ? 'B' : 'R');

    // Display user's team with color
    cout << "You are on the " << (userTeam == 'R' ? RED + "Red Team" + RESET : BLUE + "Blue Team" + RESET) << ".\n";
    cout << ((currentTeam == userTeam) ? "You start first!\n" : "Program starts first.\n");

    while (!gameEnded) {
        // Reset moved flags at the start of each round
        resetPlayersMovedFlag();

        displayBoardWithCursor(-1, -1, -1); // Display the board

        if (currentTeam == userTeam) {
            userTurn();
            if (gameEnded || checkEndConditions()) break;
            currentTeam = (userTeam == 'R' ? 'B' : 'R'); // Switch to program's team
        } else {
            programTurn();
            if (gameEnded || checkEndConditions()) break;
            currentTeam = userTeam; // Switch back to user's team
        }

        turns++;
    }
    cout << "Game over! Winner: " << winner << ". Total turns: " << turns << "\n";

    displayGameOverScreen(); // Display Game Over screen

    if (gameEnded) {
        // **Play game over sound effect**
        Mix_PlayChannel(-1, gameoverSound, 0);
    }
}


// Start of Selection
void Game::userTurn() {
    bool validTurn = false;

    while (!validTurn) {

        vector<Player*>& teamPlayers = (userTeam == 'R' ? redTeam : blueTeam);
        vector<Player*> activePlayers;
        for (Player* p : teamPlayers) {
            if (!p->isEliminated()) {
                activePlayers.push_back(p);
            }
        }

        if (activePlayers.empty()) {
            cout << "No active players available for your turn.\n";
            validTurn = true;
            break;
        }

        // Collect cells that have active players of the user's team
        vector<pair<int, int>> teamCells;
        map<pair<int, int>, vector<Player*>> cellPlayersMap;
        for (Player* p : activePlayers) {
            pair<int, int> pos = make_pair(p->getX(), p->getY());
            if (cellPlayersMap.find(pos) == cellPlayersMap.end()) {
                teamCells.push_back(pos);
            }
            cellPlayersMap[pos].push_back(p);
        }

        int cellIndex = -1;    // Index for navigating between cells (no cell selected initially)
        int playerIndex = -1;  // Index for navigating between players in a cell (no player selected initially)

        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        while (true) {
            system("clear");

            // If a cell is selected, get its coordinates
            int cursorX = -1;
            int cursorY = -1;
            if (cellIndex >= 0 && cellIndex < (int)teamCells.size()) {
                cursorX = teamCells[cellIndex].first;
                cursorY = teamCells[cellIndex].second;
            }

            displayBoardWithCursor(cursorX, cursorY, playerIndex);

            cout << "Use arrow keys to move (UP/DOWN between cells, LEFT/RIGHT between players in cell). Press Enter to select a player.\n";

            int c = cin.get();
            if (c == '\033') { // Start of escape sequence
                cin.get();     // Skip the [
                switch (cin.get()) {
                    case 'A': // Arrow up
                        if (cellIndex > 0) {
                            cellIndex--;
                            playerIndex = 0; // Reset player index when changing cells
                        }
                        break;
                    case 'B': // Arrow down
                        if (cellIndex < (int)teamCells.size() - 1) {
                            cellIndex++;
                            playerIndex = 0; // Reset player index when changing cells
                        }
                        break;
                    case 'C': // Arrow right
                        if (cellIndex != -1) {
                            pair<int, int> pos = teamCells[cellIndex];
                            vector<Player*>& playersInCell = cellPlayersMap[pos];
                            if (playerIndex < (int)playersInCell.size() - 1) {
                                playerIndex++;
                            }
                        }
                        break;
                    case 'D': // Arrow left
                        if (cellIndex != -1) {
                            if (playerIndex > 0) {
                                playerIndex--;
                            }
                        }
                        break;
                }
            } else if ((c == '\n' || c == '\r') && cellIndex != -1 && playerIndex != -1) { // Enter key
                pair<int, int> pos = teamCells[cellIndex];
                vector<Player*>& playersInCell = cellPlayersMap[pos];
                Player* selectedPlayer = playersInCell[playerIndex];

                if (selectedPlayer && !selectedPlayer->isEliminated()) {
                    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                    cout << "Selected Player " << selectedPlayer->getId() << " at (" << selectedPlayer->getX() << ", " << selectedPlayer->getY() << ")\n";
                    char action;
                    cout << "Enter 'm' to move or 'a' to attack: ";
                    cin >> action;
                    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

                    if (action == 'm') {
                        // Movement
                        cout << "Use arrow keys to select direction to move. Press 'Esc' to cancel.\n";
                        int direction = -1;
                        while (direction == -1) {
                            int c = cin.get();
                            if (c == '\033') { // Start of escape sequence
                                cin.get();     // Skip the [
                                switch (cin.get()) {
                                    case 'A': direction = UP; break;
                                    case 'B': direction = DOWN; break;
                                    case 'C': direction = RIGHT; break;
                                    case 'D': direction = LEFT; break;
                                }
                            }
                        }
                        if (direction == -1) continue; // If action was canceled

                        int squares = selectedPlayer->getMaxMovement();

                        // Prompt the user if the player can move more than one square
                        if (squares > 1) {
                            cout << "Enter number of squares to move (1 or 2): ";
                            while (true) {
                                string input;
                                getline(cin, input);
                                try {
                                    int inputSquares = stoi(input);
                                    if (inputSquares == 1 || inputSquares == 2) {
                                        squares = inputSquares;
                                        break;
                                    }
                                } catch (const invalid_argument& e) {
                                    cout << "Invalid input. Please enter 1 or 2: ";
                                } catch (const out_of_range& e) {
                                    cout << "Invalid input. Please enter 1 or 2: ";
                                }
                            }
                        } else {
                            squares = 1;
                            cout << "This player can only move 1 square.\n";
                        }

                        std::string moveResult = selectedPlayer->move(direction, squares, board, rng);
                        cout << moveResult << "\n";
                        actionHistory.push_back(make_pair(userTeam, getCurrentTime() + " User: " + moveResult));
                        validTurn = true;

                        // juego.cpp: In userTurn() after a successful move
                        if (selectedPlayer->getTeam() == 'R') {
                            redTeamMoved = true;
                        } else {
                            blueTeamMoved = true;
                        }

                    } else if (action == 'a') {
                        // Attack
                        cout << "Use arrow keys to select attack direction. Press 'Esc' to cancel.\n";
                        int direction = -1;
                        while (direction == -1) {
                            int c = cin.get();
                            if (c == '\033') { // Start of escape sequence
                                cin.get();     // Skip the [
                                switch (cin.get()) {
                                    case 'A': direction = UP; break;
                                    case 'B': direction = DOWN; break;
                                    case 'C': direction = RIGHT; break;
                                    case 'D': direction = LEFT; break;
                                }
                            }
                        }
                        if (direction == -1) continue; // If action was canceled

                        int range = -1;
                        if (selectedPlayer->isExpert()) {
                            cout << "Enter attack range (1 or 2): ";
                            // Immediate input without pressing Enter
                            while (range == -1) {
                                int c = cin.get();
                                if (c == '1') range = 1;
                                else if (c == '2') range = 2;
                                else cout << "\nInvalid range. Enter 1 or 2: ";
                            }
                        } else {
                            range = 1; // Novice attacks at range 1
                            cout << "This is a novice player. Attack range is 1.\n";
                        }

                        pair<bool, string> attackResult = selectedPlayer->attack(direction, range, board, rng);
                        cout << attackResult.second << "\n";
                        actionHistory.push_back(make_pair(userTeam, getCurrentTime() + " User: " + attackResult.second));
                        validTurn = true;
                    } else {
                        cout << "Invalid action. Press Enter to try again.\n";
                        cin.get();
                    }

                    if (validTurn) {
                        // Play jump sound effect
                        Mix_PlayChannel(-1, jumpSound, 0);
                        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                        break; // End the turn after a valid action
                    }

                }
            } else if (c == '\033') { // Escape key
                cout << "\nEscape key pressed. Exiting the game...\n";
                gameEnded = true;
                break;
            }
        }
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

void Game::programTurn() {
    string message = "\nProgram's turn.\n";
    cout << message;
    actionHistory.push_back(make_pair((userTeam == 'R') ? 'B' : 'R', getCurrentTime() + " Computer: " + message));
    vector<Player*>& programTeam = (userTeam == 'R' ? blueTeam : redTeam);

    // Find non-eliminated players
    vector<Player*> activePlayers;
    for (Player* player : programTeam) {
        if (!player->isEliminated()) {
            activePlayers.push_back(player);
        }
    }

    if (activePlayers.empty()) {
        message = "No active players available for program's turn.\n";
        cout << message;
        actionHistory.push_back(make_pair((userTeam == 'R') ? 'B' : 'R', getCurrentTime() + " Computer: " + message));
        return;
    }

    // Prioritize capturing the opponent's flag
    pair<int, int> targetFlag = (programTeam == redTeam) ? blueFlag : redFlag;

    // Sort players based on their distance to the opponent's flag
    sort(activePlayers.begin(), activePlayers.end(), [&](Player* a, Player* b) {
        int distA = abs(a->getX() - targetFlag.first) + abs(a->getY() - targetFlag.second);
        int distB = abs(b->getX() - targetFlag.first) + abs(b->getY() - targetFlag.second);
        return distA < distB;
    });

    bool actionTaken = false;

    for (Player* player : activePlayers) {
        if (player->isEliminated()) continue;

        // Calculate the difference in positions
        int deltaX = targetFlag.first - player->getX();
        int deltaY = targetFlag.second - player->getY();

        // Determine primary and secondary directions to move towards the flag
        vector<int> moveDirections;
        if (abs(deltaX) >= abs(deltaY)) {
            if (deltaX > 0) moveDirections.push_back(RIGHT);
            else if (deltaX < 0) moveDirections.push_back(LEFT);
            if (deltaY > 0) moveDirections.push_back(DOWN);
            else if (deltaY < 0) moveDirections.push_back(UP);
        } else {
            if (deltaY > 0) moveDirections.push_back(DOWN);
            else if (deltaY < 0) moveDirections.push_back(UP);
            if (deltaX > 0) moveDirections.push_back(RIGHT);
            else if (deltaX < 0) moveDirections.push_back(LEFT);
        }

        // Check if any enemy players are within attack range
        bool attackPossible = false;
        vector<int> attackDirections = {UP, DOWN, LEFT, RIGHT};

        for (int dir : attackDirections) {
            for (int range = 1; range <= player->getAttackRange(); ++range) {
                pair<bool, string> attackResult = player->attack(dir, range, board, rng);
                if (attackResult.first) {
                    // Attack was successful
                    cout << attackResult.second << "\n";
                    actionHistory.push_back(make_pair((userTeam == 'R') ? 'B' : 'R',
                        getCurrentTime() + " Computer: " + attackResult.second));
                    if (player->isShooterEliminated()) {
                        message = "Program player " + to_string(player->getId())
                                  + " is eliminated due to headshot penalty.\n";
                        cout << message;
                        actionHistory.push_back(make_pair((userTeam == 'R') ? 'B' : 'R',
                            getCurrentTime() + " Computer: " + message));
                    }
                    actionTaken = true;
                    attackPossible = true;
                    break;
                }
            }
            if (attackPossible)
                break;
        }

        if (!attackPossible) {
            // Move towards the opponent's flag
            int maxSteps = player->getMaxMovement();

            bool moved = false;
            for (int dir : moveDirections) {
                std::string moveResult = player->move(dir, maxSteps, board, rng);
                if (moveResult.find("Player") != std::string::npos) {
                    // Movement was successful
                    cout << moveResult << "\n";
                    actionHistory.push_back(make_pair((userTeam == 'R') ? 'B' : 'R',
                        getCurrentTime() + " Computer: " + moveResult));
                    actionTaken = true;
                    moved = true;
                    break;
                }
            }

            if (!moved) {
                // If can't move towards the flag, try attacking nearby enemies
                for (int dir : attackDirections) {
                    for (int range = 1; range <= player->getAttackRange(); ++range) {
                        pair<bool, string> attackResult = player->attack(dir, range, board, rng);
                        if (attackResult.first) {
                            // Attack was successful
                            cout << attackResult.second << "\n";
                            actionHistory.push_back(make_pair((userTeam == 'R') ? 'B' : 'R',
                                getCurrentTime() + " Computer: " + attackResult.second));
                            if (player->isShooterEliminated()) {
                                message = "Program player " + to_string(player->getId())
                                          + " is eliminated due to headshot penalty.\n";
                                cout << message;
                                actionHistory.push_back(make_pair((userTeam == 'R') ? 'B' : 'R',
                                    getCurrentTime() + " Computer: " + message));
                            }
                            actionTaken = true;
                            attackPossible = true;
                            break;
                        }
                    }
                    if (attackPossible)
                        break;
                }
            }
        }

        if (actionTaken)
            break; // Exit the loop after taking an action
    }

    if (!actionTaken) {
        message = "Program couldn't perform any actions.\n";
        cout << message;
        actionHistory.push_back(make_pair((userTeam == 'R') ? 'B' : 'R',
            getCurrentTime() + " Computer: " + message));
    }

    if (actionTaken) {
        // Play jump sound effect
        Mix_PlayChannel(-1, jumpSound, 0);

        // Update the team's moved flag based on the program's team
        if (programTeam == redTeam) {
            redTeamMoved = true;
        } else {
            blueTeamMoved = true;
        }
    }
}


bool Game::checkEndConditions() {
    // Check if all players have moved at least once in the current turn
    bool redAllMoved = true;
    for (Player* p : redTeam) {
        if (!p->isEliminated() && !p->hasMoved()) {
            redAllMoved = false;
            break;
        }
    }
    bool blueAllMoved = true;
    for (Player* p : blueTeam) {
        if (!p->isEliminated() && !p->hasMoved()) {
            blueAllMoved = false;
            break;
        }
    }

    // Apply retreat logic only if all players have moved
    if (redAllMoved && blueAllMoved) {
        // Proceed with retreat checks as per your existing logic
        // ...
    }

    // Check if any team reached opponent's flag area
    for (Player* p : redTeam) {
        if (!p->isEliminated() && p->getX() == blueFlag.first && p->getY() == blueFlag.second) {
            displayBoardWithCursor(-1, -1, -1);
            cout << "\nRed Team wins by capturing Blue's flag area!\n";
            winner = "Red Team";
            gameEnded = true;
            return true;
        }
    }
    for (Player* p : blueTeam) {
        if (!p->isEliminated() && p->getX() == redFlag.first && p->getY() == redFlag.second) {
            displayBoardWithCursor(-1, -1, -1);
            cout << "\nBlue Team wins by capturing Red's flag area!\n";
            winner = "Blue Team";
            gameEnded = true;
            return true;
        }
    }

    // Check if all players of one team are eliminated
    bool redEliminated = true;
    bool blueEliminated = true;

    for (Player* p : redTeam) {
        if (!p->isEliminated()) {
            redEliminated = false;
            break;
        }
    }
    for (Player* p : blueTeam) {
        if (!p->isEliminated()) {
            blueEliminated = false;
            break;
        }
    }

    if (redEliminated) {
        displayBoardWithCursor(-1, -1, -1);
        cout << "\nBlue Team wins by eliminating all Red Team players!\n";
        winner = "Blue Team";
        gameEnded = true;
        return true;
    }
    if (blueEliminated) {
        displayBoardWithCursor(-1, -1, -1);
        cout << "\nRed Team wins by eliminating all Blue Team players!\n";
        winner = "Red Team";
        gameEnded = true;
        return true;
    }

    // Check if only players in flag area remain
    bool redOnlyAtFlag = true;
    for (Player* p : redTeam) {
        if (!p->isEliminated() && 
            (p->getX() != redFlag.first || p->getY() != redFlag.second)) {
            redOnlyAtFlag = false;
            break;
        }
    }
    bool blueOnlyAtFlag = true;
    for (Player* p : blueTeam) {
        if (!p->isEliminated() && 
            (p->getX() != blueFlag.first || p->getY() != blueFlag.second)) {
            blueOnlyAtFlag = false;
            break;
        }
    }

    if (redOnlyAtFlag && !redEliminated) {
        cout << "\nAll active Red Team players are at their flag area. Blue Team wins by opponent's retreat!\n";
        winner = "Blue Team";
        gameEnded = true;
        return true;
    }
    if (blueOnlyAtFlag && !blueEliminated) {
        cout << "\nAll active Blue Team players are at their flag area. Red Team wins by opponent's retreat!\n";
        winner = "Red Team";
        gameEnded = true;
        return true;
    }

    return false;
}

Game::~Game() {
    for (Player* p : redTeam)
        delete p;
    for (Player* p : blueTeam)
        delete p;

    // Stop the music
    Mix_HaltMusic();

    // Free the music and sound effects
    if (bgm) {
        Mix_FreeMusic(bgm);
        bgm = nullptr;
    }
    if (jumpSound) {
        Mix_FreeChunk(jumpSound);
        jumpSound = nullptr;
    }
    if (gameoverSound) {
        Mix_FreeChunk(gameoverSound);
        gameoverSound = nullptr;
    }

    // Close the audio device
    Mix_CloseAudio();

    // Quit SDL subsystems
    SDL_Quit();
}

 // Start of Selection
void Game::displayBoardWithCursor(int cursorX, int cursorY, int playerIndex) {
    const string RED = "\033[31m";
    const string BLUE = "\033[34m";
    const string BRIGHT_RED = "\033[91m";
    const string BRIGHT_BLUE = "\033[94m";
    const string RESET = "\033[0m";
    const string HIGHLIGHT = "\033[43m"; // Yellow background for highlight

    const int cellWidth = 20;   // Adjusted cell width as needed
    const int cellHeight = 6;   // Adjusted cell height to accommodate coordinates

    cout << "\nCurrent Board State:\n";

    // Top border
    cout << "+";
    for (size_t x = 0; x < board[0].size(); ++x) {
        cout << string(cellWidth, '-');
        cout << "+";
    }
    cout << "\n";

    for (size_t y = 0; y < board.size(); ++y) {
        // For each line in the cell height
        for (int h = 0; h < cellHeight; ++h) {
            cout << "|";
            for (size_t x = 0; x < board[y].size(); ++x) {
                const vector<Player*>& playersInCell = board[y][x].getPlayers();
                
                // **Filter out eliminated players**
                vector<Player*> activePlayersInCell;
                for (Player* p : playersInCell) {
                    if (!p->isEliminated()) {
                        activePlayersInCell.push_back(p);
                    }
                }

                string cellContent;
                if (h < (int)activePlayersInCell.size()) {
                    // Display active players
                    Player* p = activePlayersInCell[h];
                    string teamColor;
                    if (p->getTeam() == 'R') {
                        teamColor = (p->getTeam() == userTeam) ? BRIGHT_RED : RED;
                    } else {
                        teamColor = (p->getTeam() == userTeam) ? BRIGHT_BLUE : BLUE;
                    }
                    string playerRepresentation = p->getEmojiRepresentation();

                    // Highlight only the individual selected player
                    if ((int)x == cursorX && (int)y == cursorY && playerIndex == h) {
                        playerRepresentation = HIGHLIGHT + playerRepresentation + RESET;
                    }

                    string content = teamColor + playerRepresentation + RESET;

                    // Ensure the player representation line keeps the same width
                    int contentDisplayWidth = getDisplayWidth(content);
                    int paddingTotal = cellWidth - 10;
                    if (paddingTotal > 0) {
                        int paddingLeft = paddingTotal / 2;
                        int paddingRight = paddingTotal - paddingLeft;
                        content = string(paddingLeft, ' ') + content + string(paddingRight, ' ');
                    } else {
                        // Trim content if it's too long
                        content = content.substr(0, cellWidth);
                    }
                    cellContent = content;
                } else if (h == cellHeight - 1) {
                    // Display coordinates at the bottom of the cell
                    string coord = "(" + to_string(x) + "," + to_string(y) + ")";
                    // Center align the coordinate
                    int contentDisplayWidth = getDisplayWidth(coord);
                    int paddingTotal = cellWidth - contentDisplayWidth;
                    if (paddingTotal > 0) {
                        int paddingLeft = paddingTotal / 2;
                        int paddingRight = paddingTotal - paddingLeft;
                        coord = string(paddingLeft, ' ') + coord + string(paddingRight, ' ');
                    } else {
                        coord = coord.substr(0, cellWidth);
                    }
                    cellContent = coord;
                } else {
                    // Empty line
                    cellContent = string(cellWidth, ' ');
                }

                cout << cellContent << "|";
            }
            cout << "\n";
        }

        // Add horizontal separator between rows
        cout << "+";
        for (size_t x = 0; x < board[0].size(); ++x) {
            cout << string(cellWidth, '-');
            cout << "+";
        }
        cout << "\n";
    }

    // Display selected player information
    if (cursorX >= 0 && cursorY >= 0 && cursorY < (int)board.size() && cursorX < (int)board[0].size()) {
        const vector<Player*>& playersInCell = board[cursorY][cursorX].getPlayers();
        if (playerIndex >= 0 && playerIndex < (int)playersInCell.size()) {
            Player* selectedPlayer = playersInCell[playerIndex];
            string teamColor = (selectedPlayer->getTeam() == 'R') 
                ? ((selectedPlayer->getTeam() == userTeam) ? BRIGHT_RED : RED) 
                : ((selectedPlayer->getTeam() == userTeam) ? BRIGHT_BLUE : BLUE);
            cout << "Selected Player: " << teamColor
                 << selectedPlayer->getEmojiRepresentation() << RESET
                 << " (ID: " << selectedPlayer->getId()
                 << ", Team: " << (selectedPlayer->getTeam() == 'R' ? RED + "Red" + RESET : BLUE + "Blue" + RESET)
                 << ", Position: (" << selectedPlayer->getX() << ", " << selectedPlayer->getY() << "))\n";
        } else {
            cout << "No player selected. Current cursor position: (" << cursorX << ", " << cursorY << ")\n";
        }
    }

    // Display action history with colored team actions
    cout << "\nAction History:\n";
    int startIdx = max(0, (int)actionHistory.size() - 3);
    for (int i = startIdx; i < (int)actionHistory.size(); ++i) {
        char actionTeam = actionHistory[i].first;
        const string& action = actionHistory[i].second;

        string coloredAction;
        if (actionTeam == 'R') {
            coloredAction = RED + action + RESET;
        } else if (actionTeam == 'B') {
            coloredAction = BLUE + action + RESET;
        } else {
            coloredAction = action;
        }
        cout << coloredAction << "\n";
    }

    // **Display team stats**
    cout << "\n\033[1mTeam Stats:\033[0m\n";

    // Collect eliminated players with reasons
    vector<string> redEliminatedPlayers;
    vector<string> blueEliminatedPlayers;

    int redActive = 0, blueActive = 0;
    for (Player* p : redTeam) {
        if (p->isEliminated()) {
            redEliminatedPlayers.push_back(
                to_string(p->getId()) + " (" + p->getEliminationReason() + ")");
        } else {
            redActive++;
        }
    }
    for (Player* p : blueTeam) {
        if (p->isEliminated()) {
            blueEliminatedPlayers.push_back(
                to_string(p->getId()) + " (" + p->getEliminationReason() + ")");
        } else {
            blueActive++;
        }
    }

    // Display Red Team stats
    cout << RED << "Red Team - Active: " << redActive << RESET << "\n";
    if (!redEliminatedPlayers.empty()) {
        cout << RED << "Eliminated: ";
        for (const string& playerInfo : redEliminatedPlayers) {
            cout << playerInfo << "  ";
        }
        cout << RESET << "\n";
    }

    // Display Blue Team stats
    cout << BLUE << "Blue Team - Active: " << blueActive << RESET << "\n";
    if (!blueEliminatedPlayers.empty()) {
        cout << BLUE << "Eliminated: ";
        for (const string& playerInfo : blueEliminatedPlayers) {
            cout << playerInfo << "  ";
        }
        cout << RESET << "\n";
    }

    // Display current user's team color
    cout << "\nYou are on the " << (userTeam == 'R' ? BRIGHT_RED + "Red Team" + RESET : BRIGHT_BLUE + "Blue Team" + RESET) << ".\n";
}

int Game::getVisibleLength(const string& s) const {
    int length = 0;
    bool inEscape = false;
    for (size_t i = 0; i < s.length(); ++i) {
        if (s[i] == '\033') {
            inEscape = true;
        } else if (inEscape && s[i] == 'm') {
            inEscape = false;
        } else if (!inEscape) {
            length++;
        }
    }
    return length;
}

void Game::displaySplashScreen() {
    const vector<string> splashFrames = {
        "  ______          _           _ _ ",
        " |  ____|        | |         | | |",
        " | |__ _   _ _ __| |__   __ _| | |",
        " |  __| | | | '_ \\  _ \\ / _` | | |",
        " | |  | |_| | | | | |_) | (_| | | |",
        " |_|   \\__,_|_| |_|_.__/ \\__,_|_|_|",
        "                                   ",
        "                                   ",
        "    ⚽  ⚽  ⚽  ⚽  ⚽  ⚽  ⚽  ⚽    ",
        "                                   ",
        "  ⚽  ⚽  ⚽  ⚽  ⚽  ⚽  ⚽  ⚽  ⚽  ",
        "                                   ",
        "    ⚽  ⚽  ⚽  ⚽  ⚽  ⚽  ⚽    ",
        "                                   ",
        "  ⚽  ⚽  ⚽  ⚽  ⚽  ⚽  ⚽  ⚽  ⚽  ",
        "                                   "
    };

    for (int i = 0; i < 3; ++i) {
        system("clear");
        cout << "\033[1;31m";
        for (const string& line : splashFrames) {
            cout << line << "\n";
        }
        cout << "\033[0m";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    system("clear");
}

void Game::displayGameOverScreen() {
    system("clear");
    const vector<string> gameOverText = {
        "   _____                        ____                 ",
        "  / ____|                      / __ \\                ",
        " | |  __  __ _ _ __ ___   ___ | |  | |_   _____ _ __ ",
        " | | |_ |/ _` | '_ ` _ \\ / _ \\| |  | \\ \\ / / _ \\ '__|",
        " | |__| | (_| | | | | | |  __/| |__| |\\ V /  __/ |   ",
        "  \\_____|\\__,_|_| |_| |_|\\___| \\____/  \\_/ \\___|_|   "
    };

    cout << "\033[1;31m";
    for (const string& line : gameOverText) {
        cout << line << "\n";
    }
    cout << "\033[0m";
}

void Game::animateText(const string& text) {
    for (char c : text) {
        cout << c << flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    cout << endl;
}

void Game::displayAttackAnimation(int fromX, int fromY, int toX, int toY) {
    int dx = toX - fromX;
    int dy = toY - fromY;
    int steps = max(abs(dx), abs(dy));
    double xIncrement = dx / (double)steps;
    double yIncrement = dy / (double)steps;
    double x = fromX;
    double y = fromY;

    for (int i = 0; i <= steps; ++i) {
        system("clear");
        displayBoardWithCursor(-1, -1, -1);
        int projX = round(x);
        int projY = round(y);
        if (projY >= 0 && projY < board.size() && projX >= 0 && projX < board[0].size()) {
            cout << "\033[" << projY + 3 << ";" << (projX * 15 + 2) << "H" << " o ";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        x += xIncrement;
        y += yIncrement;
    }
}

void Game::playMusic(const std::string& musicFilePath) {
    // Initialize SDL2
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return;
    }

    // Initialize SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << std::endl;
        SDL_Quit();
        return;
    }

    // Load music file
    bgm = Mix_LoadMUS(musicFilePath.c_str());
    if (!bgm) {
        std::cerr << "Failed to load music! SDL_mixer Error: " << Mix_GetError() << std::endl;
        Mix_CloseAudio();
        SDL_Quit();
        return;
    }

    // Play music
    if (Mix_PlayMusic(bgm, -1) == -1) { // -1 to loop indefinitely
        std::cerr << "Mix_PlayMusic failed: " << Mix_GetError() << std::endl;
    }

    // Note: Remove the blocking cin.get() call
    // The function now starts the music and returns immediately
}

void Game::stopMusic() {
    // Stop the music
    Mix_HaltMusic();

    // Free the music
    if (bgm) {
        Mix_FreeMusic(bgm);
        bgm = nullptr;
    }

    // Close the audio device
    Mix_CloseAudio();

    // Quit SDL subsystems
    SDL_Quit();
}

int Game::getDisplayWidth(const string& s) const {
    int width = 0;
    bool inEscape = false;
    for (size_t i = 0; i < s.length(); ) {
        if (s[i] == '\033') {
            // Skip ANSI escape sequences
            inEscape = true;
            i++;
            while (i < s.length() && !(s[i] >= '@' && s[i] <= '~')) {
                i++;
            }
            if (i < s.length()) i++; // Skip the final character of the escape sequence
            inEscape = false;
        } else if ((s[i] & 0xF8) == 0xF0) {
            // 4-byte UTF-8 character (e.g., emoji)
            width += 2; // Emojis generally occupy two character widths
            i += 4;
        } else if ((s[i] & 0xF0) == 0xE0) {
            // 3-byte UTF-8 character
            width += 1;
            i += 3;
        } else if ((s[i] & 0xE0) == 0xC0) {
            // 2-byte UTF-8 character
            width += 1;
            i += 2;
        } else {
            // 1-byte UTF-8 character
            width += 1;
            i += 1;
        }
    }
    return width;
}

void Game::resetPlayersMovedFlag() {
    for (Player* p : redTeam) {
        p->resetMoved();
    }
    for (Player* p : blueTeam) {
        p->resetMoved();
    }
}

int main() {
    Game game;
    game.play();
    return 0;
}