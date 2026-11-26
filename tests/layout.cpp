#include "../src/layout.h"
#include <limits>

int main()
{
    {
        layout::Box boxes[3];
        const double small[]={1.5,1.5,1.5};
        layout::stack({0,0,100,100},0.0,small,boxes);
        const layout::Rect last=boxes[2];
        if (boxes[2].top!=3.0 || boxes[2].bottom!=4.5 || last.top!=3 || last.bottom!=5) return 6;
        const layout::FractionalTrack equal[]={{0,1},{0,1},{0,1}};
        layout::grid({0,0,100,10},0.0,equal,boxes);
        if (std::abs(boxes[0].right-100.0/3)>1e-10 || std::abs(boxes[2].right-100)>1e-10) return 7;
        const layout::Rect first=boxes[0],second=boxes[1],third=boxes[2];
        if (first.right!=second.left || second.right!=third.left || third.right!=100) return 8;
        const layout::FractionalTrack intrinsic[]={{4.25,1},{7.5,2}};
        layout::grid({0,0,std::numeric_limits<double>::infinity(),10},.25,intrinsic,boxes);
        if (boxes[0].right!=4.25 || boxes[1].left!=4.5 || boxes[1].right!=12) return 9;
    }
    layout::Rect cells[3];
    const int heights[] = {12, 8, 15};
    layout::stack({4, 3, 60, 60}, 2, heights, cells);
    if (cells[0].top != 3 || cells[1].top != 17 || cells[2].bottom != 42) {
        return 1;
    }
    const layout::Track columns[] = {{10, 0}, {7, 1}, {9, 2}};
    layout::grid({0, 0, 100, 20}, 5, columns, cells);
    if (cells[0].right != 10 || cells[1].left != 15 ||
        cells[1].right != 41 || cells[2].right != 100) {
        return 2;
    }
    layout::grid({0, 0, -1, 20}, 5, columns, cells);
    if (cells[1].right != 22 || cells[2].right != 36) {
        return 3;
    }
    const layout::Track items[] = {{20, 0}, {0, 1}};
    layout::flex({0, 0, 55, 20}, 5, items, cells);
    if (cells[0].right != 20 || cells[1].left != 25 || cells[1].right != 55) {
        return 4;
    }
    const auto scrolled = layout::scroll({2, 5, 40, 35}, 45, 99);
    return scrolled.top == -10 && scrolled.bottom == 35 &&
        layout::scroll_limit(30, 12) == 0 ? 0 : 5;
}
