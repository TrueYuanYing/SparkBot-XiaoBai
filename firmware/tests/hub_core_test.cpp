#include "hub_core.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    auto diagonal=sparkbot::RemoteDrive(0.28f,0.28f);
    // Factory decodes left = y+x, right = y-x. Keep both tracks driving forward.
    assert(diagonal.y-diagonal.x > 0.15f);
    auto mirrored=sparkbot::RemoteDrive(-0.28f,0.28f);
    assert(std::abs(diagonal.x+mirrored.x)<.0001f && std::abs(diagonal.y-mirrored.y)<.0001f);
    auto centered=sparkbot::RemoteDrive(.01f,-.01f);
    assert(centered.x==0 && centered.y==0);
    auto pivot=sparkbot::RemoteDrive(.4f,0);
    assert(std::abs(pivot.x-.4f)<.0001f && pivot.y==0);
    for(int i=-100;i<=100;++i)for(int j=-100;j<=100;++j) {
        float x=i/100.f,y=j/100.f;
        auto v=sparkbot::RemoteDrive(x,y),mirror=sparkbot::RemoteDrive(-x,y);
        assert(std::abs(v.y+v.x)<=.9801f && std::abs(v.y-v.x)<=.9801f);
        assert(std::abs(v.x+mirror.x)<.0001f && std::abs(v.y-mirror.y)<.0001f);
        if(y>.05f && y>=std::abs(x)*.4f)assert(v.y-v.x>0 && v.y+v.x>0);
        if(y<-.05f && -y>=std::abs(x)*.4f)assert(v.y-v.x<0 && v.y+v.x<0);
    }
    std::cout << "PASS: symmetric arcs keep both tracks engaged, pivot, dead zone and PWM limits\n";
    sparkbot::Motion motion;
    assert(motion.Move(7, 1, 0.0f, 0.7f, 100));
    assert(motion.At(449).y == 0.7f);
    assert(motion.At(450).y == 0.0f);
    std::cout << "PASS: lost control heartbeat stops at 350 ms\n";
    assert(!motion.Move(7, 1, 1, 1, 451));
    assert(!motion.Move(7, 2, NAN, 1, 451));
    assert(motion.Move(7, 2, 2, -2, 452));
    assert(motion.At(453).x == 1 && motion.At(453).y == -1);
    assert(!motion.Move(8, 1, 0, 1, 453));
    motion.EmergencyStop();
    assert(motion.At(454).x == 0);
    assert(!motion.Move(7, 3, 0, 1, 455));
    motion.Arm();
    assert(motion.At(456).y == 0);
    assert(motion.Move(7, 3, 0, 1, 457));
    motion.Disconnect(8);
    assert(motion.At(458).y == 1);
    motion.Disconnect(7);
    assert(motion.At(459).y == 0);
    assert(motion.Move(8, 1, -0.5f, 0, 460));
    assert(!motion.Move(8, 2, 0, INFINITY, 461));
    assert(!motion.Move(8, 2, 0, 1, 461, 1501));
    motion.Stop();
    assert(motion.At(462).x == 0);
    std::cout << "PASS: stale/invalid/concurrent control rejected; emergency stop latched\n";
    sparkbot::Game2048 game;
    game.SetBoard({2,2,2,2, 0,0,0,0, 0,0,0,0, 0,0,0,0});
    assert(game.Slide(sparkbot::Direction::Left));
    assert((game.Cells() == std::array<uint32_t,16>{4,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}));
    assert(game.Score() == 8);
    assert(game.Slide(sparkbot::Direction::Right));
    assert(game.Cells()[3] == 8 && game.Score() == 16);
    assert(!game.Slide(sparkbot::Direction::Right));
    game.SetBoard({2,0,0,0, 2,0,0,0, 4,0,0,0, 4,0,0,0});
    assert(game.Slide(sparkbot::Direction::Down));
    assert(game.Cells()[12] == 8 && game.Cells()[8] == 4);
    assert(game.Slide(sparkbot::Direction::Up));
    assert(game.Cells()[0] == 4 && game.Cells()[4] == 8);
    game.SetBoard({2,4,2,4, 4,2,4,2, 2,4,2,4, 4,2,4,2});
    assert(game.GameOver());
    auto full = game.Cells();
    assert(!game.Spawn(123) && game.Cells() == full);
    game.SetBoard({2,4,2,4, 4,2,4,2, 2,4,2,4, 4,2,4,4});
    assert(!game.GameOver());
    game.SetBoard({});
    assert(game.Spawn(21));
    unsigned occupied = 0;
    for (auto cell : game.Cells()) { if (cell) {++occupied; assert(cell == 2 || cell == 4);} }
    assert(occupied == 1);
    std::cout << "PASS: 2048 merges each tile once and invalid moves do not score\n";
}
