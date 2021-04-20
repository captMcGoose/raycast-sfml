//Credits for most of the code to Thomas van der Berg

/*
Cheap wolfenstein chinese copycat engine in C++

#THINGS TO DO:# 
-Add mouselook
-Find out how to compile-in modules (for now make classes in main.cpp)
-Continue https://lodev.org/cgtutor/raycasting.html#Introduction and learn how to make floor and ceiling textures
-Make Sprite class

-Make level transitioning
-Implement Dijkstra/A* pathfinding


*/



#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <unordered_map>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <fstream>
#include <SFML/Window/Mouse.hpp>

// screen resolution
const int screenWidth = 1280;
const float fscreenHalf = 640.0f;
const int screenHeight = 720;

const float cameraHeight = 0.45f; //0 --> floor 1 --> ceiling

const int texture_size = 512; 
const int texture_wall_size = 128; 

const float fps_refresh_time = 0.1f; 







///////////////////////////////////////////////////////////////////////////
class Map{
    public:
        void loadMap(const std::string &name){
            std::ifstream f1;
            std::string cad;
            f1.open(name);
            int temp = 0;
            if(!f1.good()){
                std::cout << "Error" << std::endl;
                exit(1);
            }
            while(!f1.eof()){
                getline(f1, cad);
                for(int i = 0; i < cad.length(); i++){
                    worldMap[i+temp] = cad[i];
                }
                temp += cad.length();
            }

            f1.close();
        }
        
        enum class WallTexture {
            Banner,
            Red,
            Mutant,
            Gray,
            Blue,
            Moss,
            Wood,
            Stone,
        };

        char getTile(int x, int y) const{
            return worldMap[y*mapWidth+x];
        }
        
        bool mapCheck(){
            int mSize = 24*24 - 1; // -1 idkw

            for (int i = 0; i < mapHeight; i++) {
                for (int j = 0; j < mapWidth; j++) {
                    char tile = getTile(j, i);

                    if (tile != '.' && wallTypes.find(tile) == wallTypes.end()) {
                        std::cout << "ERROR: Unknown tile" << std::endl;
                        return false;
                    }
                    
                    if ((i == 0 || j == 0 || i == mapHeight - 1 || j == mapWidth - 1) && tile == '.') {
                        std::cout << "WARNING: Player can access limbo" << std::endl;
                        return false;
                    }
                }
            }
            return true;
        }

        const std::unordered_map<char, WallTexture> wallTypes {
            {'#', WallTexture::Blue},
            {'=', WallTexture::Wood},
            {'M', WallTexture::Moss},
            {'N', WallTexture::Mutant},
            {'A', WallTexture::Gray},
            {'!', WallTexture::Red},
            {'@', WallTexture::Banner},
            {'^', WallTexture::Stone},
        };
    
    private:

        static const int mapWidth = 24;
        static const int mapHeight = 24;
        typedef char MArray[mapWidth*mapHeight];
        MArray worldMap;
};
////////////////////////////////////////////////////////////////////////



class Object{
    public:
        Object(float xPos, float yPos, const std::string &texturePath){
            texture.loadFromFile(texturePath);
            position = sf::Vector2f(xPos, yPos);
            textureWidth = texture.getSize().x;
        }

    private:
        sf::Vector2f position;
        sf::Texture texture;
        float textureWidth;
    

};


bool canMove(sf::Vector2f position, sf::Vector2f size, const Map &map) {
    // create the corners of the rectangle
    sf::Vector2i upper_left(position - size / 2.0f);
    sf::Vector2i lower_right(position + size / 2.0f);
    
    // loop through each map tile within the rectangle. The rectangle could be multiple tiles in size!
    for (int y = upper_left.y; y <= lower_right.y; ++y) {
        for (int x = upper_left.x; x <= lower_right.x; ++x) {
            if (map.getTile(x, y) != '.') {
                return false;
            }
        }
    }
    return true;
}


sf::Vector2f rotateVec(sf::Vector2f vec, float value) {
    return sf::Vector2f(
            vec.x * std::cos(value) - vec.y * std::sin(value),
            vec.x * std::sin(value) + vec.y * std::cos(value)
    );
}

int main() {
    Map map;
    map.loadMap("level1.txt");
    if (!map.mapCheck()) {
        std::cout << "ERROR: MAP NOT VALID" << std::endl;
        char x;
       std::cin >> x;
    }

    sf::Font font;
    if (!font.loadFromFile("data/font/opensans.ttf")) {
        fprintf(stderr, "Cannot open font!\n");
        return EXIT_FAILURE;
    }

    sf::Texture texture;
    if (!texture.loadFromFile("data/texture/walls.png")) {
        fprintf(stderr, "Cannot open texture!\n");
        return EXIT_FAILURE;
    }

    // render state that uses the texture
    sf::RenderStates state(&texture);

    // player
    sf::Vector2f position(2.5f, 2.0f); 
    sf::Vector2f direction(0.0f, 1.0f);
    sf::Vector2f plane(-0.66f, 0.0f); // 2d raycaster  camera plane,
    float size_f = 0.375f; 
    float moveSpeed = 5.0f; 
    float rotateSpeed = 2.0f;

    sf::Vector2f size(size_f, size_f);
    sf::Vector2i mPos(0.0f, 0.0f);
    sf::Vector2i mPos2(fscreenHalf, 0.0f);
    

    // create window
    sf::RenderWindow window(sf::VideoMode(screenWidth + 1, screenHeight), "Wolfenstein 3D Chinese copycat");
    window.setSize(sf::Vector2u(screenWidth, screenHeight)); 

    window.setMouseCursorVisible(false);

    window.setFramerateLimit(60);
    bool hasFocus = true;

    // lines used to draw walls and floors on the screen
    sf::VertexArray lines(sf::Lines, 18 * screenWidth);

    sf::Text fpsText("", font, 50); // text object for FPS counter
    sf::Clock clock; // timer
    char frameInfoString[sizeof("FPS: *****.*, Frame time: ******")]; // string buffer for frame information

    float dt_counter = 0.0f; // delta time
    int frame_counter = 0; // counts frames for FPS calculation
    int64_t frame_time_micro = 0; // time needed to draw frames in microseconds

    sf::Texture objTexture;
    objTexture.loadFromFile("data/texture/barrel.png");
    sf::Sprite sprite;
    sprite.setTexture(objTexture, true);
    sprite.setPosition(640.0f, 700.0f);

    
    Object npc(2.0f, 2.5f, "data/texture/barrel.png");

    

    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

        // Update FPS, smoothed over time
        if (dt_counter >= fps_refresh_time) {
            float fps = (float)frame_counter / dt_counter;
            frame_time_micro /= frame_counter;
            snprintf(frameInfoString, sizeof(frameInfoString), "FPS: %3.1f, Frame time: %6ld", fps, frame_time_micro);
            fpsText.setString(frameInfoString);
            dt_counter = 0.0f;
            frame_counter = 0;
            frame_time_micro = 0;
        }
        dt_counter += dt;
        ++frame_counter;

        //SFML events
        sf::Event event;
        while (window.pollEvent(event)) {
            switch(event.type) {
            case sf::Event::Closed:
                window.close();
                break;
            case sf::Event::LostFocus:
                hasFocus = false;
                break;
            case sf::Event::GainedFocus:
                hasFocus = true;
                break;
            default:
                break;
            }
        }
        
        //keyboard input
        if (hasFocus) {

            
            using kb = sf::Keyboard;

            float moveForward = 0.0f;
            if (kb::isKeyPressed(kb::W)) {
                moveForward = 1.0f;
            } else if (kb::isKeyPressed(kb::S)) {
                moveForward = -1.0f;
            }

            if (moveForward != 0.0f) {
                sf::Vector2f moveVec = direction * moveSpeed * moveForward * dt;

                if (canMove(sf::Vector2f(position.x + moveVec.x, position.y), size, map)) {
                    position.x += moveVec.x;
                }
                if (canMove(sf::Vector2f(position.x, position.y + moveVec.y), size, map)) {
                    position.y += moveVec.y;
                }
            }

            float strafe = 0.0f;

            if (kb::isKeyPressed(kb::D)) {
                strafe = 1.0f;
            } else if (kb::isKeyPressed(kb::A)) {
                strafe = -1.0f;
            }

            if (strafe != 0.0f) {
                sf::Vector2f moveVec = sf::Vector2f(direction.x * std::cos(90*strafe) - direction.y * std::sin(90*strafe), direction.x * std::sin(90*strafe) + direction.y * std::cos(90*strafe)) * moveSpeed * dt;
                

                if (canMove(sf::Vector2f(position.x + moveVec.x, position.y), size, map)) {
                    position.x += moveVec.x;
                }
                if (canMove(sf::Vector2f(position.x, position.y + moveVec.y), size, map)) {
                    position.y += moveVec.y;
                }
            }

            float rotateDirection = 0.0f;

         

            if(sf::Mouse::getPosition(window).x > mPos.x){
                rotateDirection = 1.0f;
            } else if(sf::Mouse::getPosition(window).x < mPos.x){
                rotateDirection = -1.0f;
            }
            mPos = sf::Mouse::getPosition(window);

            if(mPos.x >= (float)screenWidth || mPos.x <= (float)0 || mPos.y >= (float)screenHeight || mPos.y <= (float)0){
                sf::Mouse::setPosition(mPos2, window);
                mPos = sf::Mouse::getPosition(window);
            }

            // handle rotation
            if (rotateDirection != 0.0f) {
                float rotation = rotateSpeed * rotateDirection * dt;
                direction = rotateVec(direction, rotation);
                plane = rotateVec(plane, rotation);
            }

            //Close application
            if (kb::isKeyPressed(kb::Escape)) {
                exit(0);
            }
        }









        //Here comes the rendering.... 

        lines.resize(0);

        // loop through vertical screen lines, draw a line of wall for each
        for (int x = 0; x < screenWidth; ++x) {

            // ray to emit
            float cameraX = 2 * x / (float)screenWidth - 1.0f; // x in camera space (between -1 and +1)
            sf::Vector2f rayPos = position;
            sf::Vector2f rayDir = direction + plane * cameraX;

            //https://lodev.org/cgtutor/raycasting.html#Introduction
            // NOTE: with floats, division by zero gives you the "infinity" value. This code depends on this.

            // calculate distance traversed between each grid line for x and y based on direction
            sf::Vector2f deltaDist(
                    sqrt(1.0f + (rayDir.y * rayDir.y) / (rayDir.x * rayDir.x)),
                    sqrt(1.0f + (rayDir.x * rayDir.x) / (rayDir.y * rayDir.y))
            );

            sf::Vector2i mapPos(rayPos); // which box of the map we are in

            sf::Vector2i step; // what direction to step in (+1 or -1 for each dimension)
            sf::Vector2f sideDist; // distance from current position to next gridline, for x and y separately

            // calculate step and initial sideDist
            if (rayDir.x < 0.0f) {
                step.x = -1;
                sideDist.x = (rayPos.x - mapPos.x) * deltaDist.x;
            } else {
                step.x = 1;
                sideDist.x = (mapPos.x + 1.0f - rayPos.x) * deltaDist.x;
            }
            if (rayDir.y < 0.0f) {
                step.y = -1;
                sideDist.y = (rayPos.y - mapPos.y) * deltaDist.y;
            } else {
                step.y = 1;
                sideDist.y = (mapPos.y + 1.0f - rayPos.y) * deltaDist.y;
            }

            char tile = '.'; // tile type that got hit
            bool horizontal; //check if we hit a horizontal side

            float perpWallDist = 0.0f; // wall distance
            int wallHeight; 
            int ceilingPixel = 0; // position of ceiling pixel on the screen
            int groundPixel = screenHeight; // position of ground pixel on the screen

            // colors for floor tiles
            sf::Color color1 = sf::Color::Blue;
            sf::Color color2 = sf::Color::White;
            sf::Color color3 = sf::Color::White;

            // current floor color
            sf::Color color = ((mapPos.x % 2 == 0 && mapPos.y % 2 == 0) ||
                               (mapPos.x % 2 == 1 && mapPos.y % 2 == 1)) ? color1 : color2;

            // cast the ray until we hit a wall, meanwhile draw floors
            while (tile == '.') {
                if (sideDist.x < sideDist.y) {
                    sideDist.x += deltaDist.x;
                    mapPos.x += step.x;
                    horizontal = true;
                    perpWallDist = (mapPos.x - rayPos.x + (1 - step.x) / 2) / rayDir.x;
                } else {
                    sideDist.y += deltaDist.y;
                    mapPos.y += step.y;
                    horizontal = false;
                    perpWallDist = (mapPos.y - rayPos.y + (1 - step.y) / 2) / rayDir.y;
                }

                wallHeight = screenHeight / perpWallDist;

                // add floor

                lines.append(sf::Vertex(sf::Vector2f((float)x, (float)groundPixel), color, sf::Vector2f(385.0f, 129.0f)));
                groundPixel = int(wallHeight * cameraHeight + screenHeight * 0.5f);
                lines.append(sf::Vertex(sf::Vector2f((float)x, (float)groundPixel), color, sf::Vector2f(385.0f, 129.0f)));

                // add ceiling

                sf::Color color_c = color;
                color_c.r /= 2;
                color_c.g /= 2;
                color_c.b /= 2;

                color3.r = 173;
                color3.g = 129;
                color3.b = 26;

                lines.append(sf::Vertex(sf::Vector2f((float)x, (float)ceilingPixel), color3, sf::Vector2f(385.0f, 129.0f)));
                ceilingPixel = int(-wallHeight * (1.0f - cameraHeight) + screenHeight * 0.5f);
                lines.append(sf::Vertex(sf::Vector2f((float)x, (float)ceilingPixel), sf::Color::Black, sf::Vector2f(385.0f, 129.0f)));
                


                 //change color and find tile type

                color = (color == color1) ? color2 : color1;

                tile = map.getTile(mapPos.x, mapPos.y);
            }

            // calculate lowest and highest pixel to fill in current line
            int drawStart = ceilingPixel;
            int drawEnd = groundPixel;

            // get position of the wall texture in the full texture
            int wallTextureNum = (int)map.wallTypes.find(tile)->second;
            sf::Vector2i texture_coords(
                    wallTextureNum * texture_wall_size % texture_size,
                    wallTextureNum * texture_wall_size / texture_size * texture_wall_size
            );

            // calculate where the wall was hit
            float wall_x;
            if (horizontal) {
                wall_x = rayPos.y + perpWallDist * rayDir.y;
            } else {
                wall_x = rayPos.x + perpWallDist * rayDir.x;
            }
            wall_x -= floor(wall_x);

            // get x coordinate on the wall texture
            int tex_x = int(wall_x * float(texture_wall_size));

            // flip texture if we see it on the other side of us, this prevents a mirrored effect for the texture
            if ((horizontal && rayDir.x <= 0) || (!horizontal && rayDir.y >= 0)) {
                tex_x = texture_wall_size - tex_x - 1;
            }

            texture_coords.x += tex_x;

            // illusion of shadows by making horizontal walls darker
            color = sf::Color::White;
            if (horizontal) {
                color.r /= 2;
                color.g /= 2;
                color.b /= 2;
            }

            // add line to vertex buffer
            lines.append(sf::Vertex(
                        sf::Vector2f((float)x, (float)drawStart),
                        color,
                        sf::Vector2f((float)texture_coords.x, (float)texture_coords.y + 1)
            ));
            lines.append(sf::Vertex(
                        sf::Vector2f((float)x, (float)drawEnd),
                        color,
                        sf::Vector2f((float)texture_coords.x, (float)(texture_coords.y + texture_wall_size - 1))
            ));
        }


        
        window.clear();
        window.draw(lines, state);
        window.draw(fpsText);
        window.draw(sprite);
        frame_time_micro += clock.getElapsedTime().asMicroseconds();
        window.display();
    }

    return 0;
}
