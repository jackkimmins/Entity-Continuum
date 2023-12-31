#include <SDL.h>
#include <emscripten.h>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>
#include <iostream>
#include <random>
#include <immintrin.h>

constexpr int WIDTH = 2560;
constexpr int HEIGHT = 1440;
constexpr int MIN_RADIUS = 1;
constexpr int MAX_GROWTH_RADIUS = 10;
constexpr int SPEED = 2;
constexpr int MAX_COLOR_VALUE = 256;
constexpr int NUM_CIRCLES = 500;
constexpr double DECAY_RATE = 0.5;
constexpr int NUM_FOODS = 700;
constexpr int FOOD_RADIUS_INCREMENT = 1;

class Food;
class Cell;

// Random number engine
std::mt19937 rng(time(0));

class Food {
public:
    int x, y;

    Food() {
        std::uniform_int_distribution<int> dist(0, WIDTH - 1);
        x = dist(rng);
        dist.param(std::uniform_int_distribution<int>::param_type(0, HEIGHT - 1));
        y = dist(rng);
    }

    double distanceTo(const Cell& cell) const;
};

class Cell {
public:
    double x, y;
    int radius;
    int eatenCells = 0;
    std::array<int, 3> color;
    int directionX, directionY;

    Cell() {
        std::uniform_int_distribution<int> dist(0, 3);  // Generate number from 0 to 3
        int randNum = dist(rng);
        directionX = (randNum & 1 ? 1 : -1) * SPEED;  // Extract 0th bit
        directionY = (randNum & 2 ? 1 : -1) * SPEED;  // Extract 1st bit
        
        radius = MIN_RADIUS;

        std::uniform_int_distribution<int> dist3(0, WIDTH - radius * 2 - 1);
        x = dist3(rng) + radius;
        dist3.param(std::uniform_int_distribution<int>::param_type(0, HEIGHT - radius * 2 - 1));
        y = dist3(rng) + radius;

        std::uniform_int_distribution<int> dist4(0, MAX_COLOR_VALUE - 1);
        std::generate_n(color.begin(), 3, [&]() { return dist4(rng); });
    }

    Cell(double x_, double y_, int radius_, int directionX_, int directionY_, std::array<int, 3> color_)
        : x(x_), y(y_), radius(radius_), directionX(directionX_), directionY(directionY_), color(color_)
    {
    }

    bool isSameColor(const Cell& other) const {
        return color == other.color;
    }

    void decayRadius() {
        double decayAmount = DECAY_RATE * static_cast<double>(radius - MIN_RADIUS) / (MAX_GROWTH_RADIUS - MIN_RADIUS);
        radius = std::max(MIN_RADIUS, radius - static_cast<int>(decayAmount));
    }

    inline void updatePosition()
    {
        double normalizedSize = static_cast<double>(radius - MIN_RADIUS) / (MAX_GROWTH_RADIUS - MIN_RADIUS);
        double minSpeed = SPEED / 16.0;
        double maxSpeed = SPEED;

        double speed = maxSpeed - std::sqrt(normalizedSize) * (maxSpeed - minSpeed);  

        x += directionX * speed;
        y += directionY * speed;

        // If the cell is going out of the boundaries, then change its direction
        if (x + radius > WIDTH) {
            x = WIDTH - radius; // Set the cell's position to be at the edge of the boundary
            directionX = -1;
        }
        else if (x - radius < 0) {
            x = radius; // Set the cell's position to be at the edge of the boundary
            directionX = 1;
        }

        if (y + radius > HEIGHT) {
            y = HEIGHT - radius; // Set the cell's position to be at the edge of the boundary
            directionY = -1;
        }
        else if (y - radius < 0) {
            y = radius; // Set the cell's position to be at the edge of the boundary
            directionY = 1;
        }
    }


    inline bool isOverlapping(const Cell& other) const
    {
        int dx = x - other.x;
        int dy = y - other.y;
        int distance = std::hypot(dx, dy);

        return distance <= (radius + other.radius);

        // if (radius > other.radius)
        //     return distance <= (radius - (0.5 * other.radius));
        // else
        //     return false;
    }

    inline bool isOverlapping(const Food& food) const {
        int dx = x - food.x;
        int dy = y - food.y;
        int distance = std::hypot(dx, dy);
        return distance <= radius;
    }

    double distanceTo(const Cell& other) const
    {
        int dx = x - other.x;
        int dy = y - other.y;
        return dx * dx + dy * dy;
    }

    inline void split(std::vector<Cell>& newCells) {
        if (radius >= MAX_GROWTH_RADIUS) {
            const int newCellCount = 2;
            double angleStep = 2.0 * M_PI / newCellCount;
            std::uniform_int_distribution<int> dist(0, 3);  // Generate number from 0 to 3
            int randNum = dist(rng);
            for (int i = 0; i < newCellCount; ++i) {
                double angle = i * angleStep;
                newCells.emplace_back(
                    x + std::cos(angle) * radius,
                    y + std::sin(angle) * radius,
                    1,
                    (randNum & 1 ? 1 : -1) * SPEED,
                    (randNum & 2 ? 1 : -1) * SPEED,
                    color  // New cell has the same color as the parent cell
                );
                randNum = dist(rng);  // Generate new random number for the next new cell
            }
            radius = 1;
        }
    }
};

double Food::distanceTo(const Cell& cell) const {
    int dx = x - cell.x;
    int dy = y - cell.y;
    return dx * dx + dy * dy;
}

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static std::vector<Cell> cells(NUM_CIRCLES);
std::vector<Food> foods(NUM_FOODS);

void Render()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    for (const auto& cell : cells) {
        SDL_SetRenderDrawColor(renderer, cell.color[0], cell.color[1], cell.color[2], 255);
        int centerX = static_cast<int>(cell.x);
        int centerY = static_cast<int>(cell.y);
        int radius = cell.radius;

        int x = radius;
        int y = 0;
        int error = 1 - x;

        while (x >= y) {
            SDL_RenderDrawLine(renderer, centerX - x, centerY + y, centerX + x, centerY + y);
            SDL_RenderDrawLine(renderer, centerX - x, centerY - y, centerX + x, centerY - y);
            SDL_RenderDrawLine(renderer, centerX - y, centerY + x, centerX + y, centerY + x);
            SDL_RenderDrawLine(renderer, centerX - y, centerY - x, centerX + y, centerY - x);

            y++;
            if (error <= 0) {
                error += 2 * y + 1;
            }
            if (error > 0) {
                x--;
                error -= 2 * x + 1;
            }
        }
    }

    // Set color for rendering food
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    for (const auto& food : foods) {
        SDL_Rect foodRect;
        foodRect.x = food.x;
        foodRect.y = food.y;
        foodRect.w = 2;
        foodRect.h = 2;
        SDL_RenderFillRect(renderer, &foodRect);
    }

    SDL_RenderPresent(renderer);
}

void Update()
{
    if (foods.size() < NUM_FOODS) foods.push_back(Food());

    std::vector<Cell> newCells; // Store newly created cells

    for (auto& cell : cells) {
        Food* nearestFood = nullptr;
        Cell* nearestCell = nullptr;
        double minFoodDist = std::numeric_limits<double>::max();
        double minCellDist = MAX_GROWTH_RADIUS;

        for (auto& other : cells) {
            if (&cell == &other || cell.isSameColor(other)) continue;
            double distance = cell.distanceTo(other);
            if (distance < minCellDist) {
                minCellDist = distance;
                nearestCell = &other;
            }
        }

        for (auto& food : foods) {
            double dx = cell.x - food.x;
            double dy = cell.y - food.y;
            double distance = std::hypot(dx, dy);
            if (distance < minFoodDist) {
                minFoodDist = distance;
                nearestFood = &food;
            }
        }

        if (nearestCell != nullptr && minCellDist < minFoodDist) {
            if (cell.radius > nearestCell->radius) {
                cell.directionX = (nearestCell->x - cell.x) > 0 ? 1 : -1;
                cell.directionY = (nearestCell->y - cell.y) > 0 ? 1 : -1;
            } else if (cell.radius < nearestCell->radius) {
                cell.directionX = (nearestCell->x - cell.x) > 0 ? -1 : 1;
                cell.directionY = (nearestCell->y - cell.y) > 0 ? -1 : 1;
            }
        } else if (nearestFood != nullptr) {
            cell.directionX = (nearestFood->x - cell.x) > 0 ? 1 : -1;
            cell.directionY = (nearestFood->y - cell.y) > 0 ? 1 : -1;
        }

        cell.updatePosition();
        cell.decayRadius();
        cell.split(newCells);
    }

    // Add the newly created cells to the main cell vector
    cells.insert(cells.end(), newCells.begin(), newCells.end());

    // Consume any food items that overlap with the cells
    for (auto& cell : cells) {
        for (auto& food : foods) {
            if (cell.isOverlapping(food)) {
                cell.radius = std::min(MAX_GROWTH_RADIUS, cell.radius + FOOD_RADIUS_INCREMENT);
                food = Food(); // Replace the consumed food with a new one
            }
        }
    }

    // Iterate over the cells and "eat" any smaller ones that overlap with larger ones
    std::uniform_int_distribution<int> dist(0, 3);  // Generate number from 0 to 3
    int randNum = dist(rng);
    for (auto& eater : cells) {
        for (auto& eaten : cells) {
            if (&eater != &eaten && eater.isOverlapping(eaten) && eater.radius > eaten.radius) {
                // Only eat the cell if it is not of the same color
                if (eater.color != eaten.color) {
                    // Randomize the direction of the eater cell
                    eater.directionX = (randNum & 1 ? 1 : -1) * SPEED;
                    eater.directionY = (randNum & 2 ? 1 : -1) * SPEED;

                    // Increase the count of eaten cells
                    eater.eatenCells++;

                    // Exponential growth factor (the more the cell eats, the slower it grows)
                    // double growthFactor = 1.0 / std::sqrt(eater.eatenCells);

                    // Logarithmic growth factor (the more the cell eats, the slower it grows)
                    double growthFactor = 1.0 / std::log(eater.eatenCells + 1);

                    // Add the radius of the eaten cell to the eater's radius, scaled by the growth factor
                    eater.radius = std::min(MAX_GROWTH_RADIUS, eater.radius + static_cast<int>(eaten.radius * growthFactor));

                    eaten.radius = 0; // Mark the eaten cell for deletion by setting its radius to 0
                    randNum = dist(rng);
                }
            }
        }
    }

    // Remove the eaten cells from the list
    cells.erase(std::remove_if(cells.begin(), cells.end(), [](const Cell& cell) { return cell.radius == 0; }), cells.end());
}

int main(int argc, char** argv)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "Failed to initialize the SDL2 library\n";
        return -1;
    }

    window = SDL_CreateWindow("Entity Continuum", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    if (window == nullptr) {
        std::cout << "Failed to create window\n";
        return -1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        std::cout << "Failed to create renderer\n";
        return -1;
    }

    emscripten_set_main_loop([]() {
        Update();
        Render();
    }, 0, 1);

    return 0;
}