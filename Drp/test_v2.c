#include "testing.h"
#include "v2.h"

TestFunction(test_recti_manhattan_dist){
    TESTBEGIN();
    {
        recti a = {.x=0, .y=0, .w=1, .h=1};
        recti b = {.x=0, .y=4, .w=1, .h=1};
        int dist = recti_manhattan_dist(a, b);
        TestExpect(dist, ==, 3);
        dist = recti_manhattan_dist(b, a);
        TestExpect(dist, ==, 3);
    }
    {
        recti a = {.x=0, .y=0, .w=1, .h=1};
        recti b = {.x=4, .y=0, .w=1, .h=1};
        int dist = recti_manhattan_dist(a, b);
        TestExpect(dist, ==, 3);
        dist = recti_manhattan_dist(b, a);
        TestExpect(dist, ==, 3);
    }
    {
        recti a = {.x=0, .y=0, .w=1, .h=1};
        recti b = {.x=1, .y=1, .w=1, .h=1};
        int dist = recti_manhattan_dist(a, b);
        TestExpect(dist, ==, 1);
        dist = recti_manhattan_dist(b, a);
        TestExpect(dist, ==, 1);
    }
    {
        recti a = {.x=0, .y=0, .w=1, .h=1};
        recti b = {.x=2, .y=1, .w=1, .h=1};
        int dist = recti_manhattan_dist(a, b);
        TestExpect(dist, ==, 2);
        dist = recti_manhattan_dist(b, a);
        TestExpect(dist, ==, 2);
    }
    {
        // #...
        // ....
        // ....
        // ...#
        recti a = {.x=0, .y=0, .w=1, .h=1};
        recti b = {.x=3, .y=3, .w=1, .h=1};
        int dist = recti_manhattan_dist(a, b);
        TestExpect(dist, ==, 5);
        dist = recti_manhattan_dist(b, a);
        TestExpect(dist, ==, 5);
    }
    {
        // ##..
        // ##..
        // ....
        // ...#
        recti a = {.x=0, .y=0, .w=2, .h=2};
        recti b = {.x=3, .y=3, .w=1, .h=1};
        int dist = recti_manhattan_dist(a, b);
        TestExpect(dist, ==, 3);
        dist = recti_manhattan_dist(b, a);
        TestExpect(dist, ==, 3);
    }
    {
        // ##..
        // ##..
        // .#..
        recti a = {.x=0, .y=0, .w=2, .h=2};
        recti b = {.x=1, .y=2, .w=1, .h=1};
        int dist = recti_manhattan_dist(a, b);
        TestExpect(dist, ==, 0);
        dist = recti_manhattan_dist(b, a);
        TestExpect(dist, ==, 0);
    }
    {
        // ####
        // ####
        // ....
        // .##.
        recti a = {.x=0, .y=0, .w=4, .h=2};
        recti b = {.x=1, .y=3, .w=2, .h=1};
        int dist = recti_manhattan_dist(a, b);
        TestExpectEquals(dist, 1);
        dist = recti_manhattan_dist(b, a);
        TestExpectEquals(dist, 1);
    }
    TESTEND();
}

int main(int argc, char** argv){
    RegisterTest(test_recti_manhattan_dist);
    return test_main(argc, argv, NULL);
}
