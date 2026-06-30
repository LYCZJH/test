#include <SFML/Graphics.hpp>// SFML 图形库主头文件
#include <vector>               // 动态数组，存储多个图形
#include <optional>             // C++17 特性：可能不存在的值
#include <cmath>                // 数学函数（hypot 计算距离）
#include <string>               // 字符串
#include <memory>               // 智能指针 unique_ptr

// 工具类型枚举 
enum class ToolType {
    Circle,         // 两点画圆（直径方式）
    CircleCenter,   // 中心+半径方式
    Circle3Point,   // 三点画圆方式
    Line,
    Rectangle
};

// 工具栏按钮结构
struct ToolButton {
    sf::RectangleShape shape;
    sf::Text label;
    ToolType tool;
    bool hovered;
    bool selected;

    // 构造函数初始化
    ToolButton(float x, float y, const std::string& text, ToolType t, bool sel,
               const sf::Font& font, float bw, float bh)
        : shape({bw, bh}), // 列表初始化 RectangleShape 的大小
        label(font),//用 font 初始化 sf::Text 
        tool(t), // 初始化工具类型
        hovered(false),// 初始不悬停
        selected(sel)// 是否选中
        { // 设置按钮矩形外观
        shape.setPosition({x, y});
        shape.setFillColor(sel ? sf::Color(60, 120, 180) : sf::Color(60, 60, 60));
        shape.setOutlineColor(sel ? sf::Color(100, 180, 255) : sf::Color(100, 100, 100));
        shape.setOutlineThickness(2.0f);
            // 设置按钮文本外观
        label.setString(text);
        label.setCharacterSize(16);
        label.setFillColor(sf::Color::White);
            // 设置按钮文字居中显示
        sf::FloatRect textBounds = label.getLocalBounds();
        label.setPosition({
            x + (bw - textBounds.size.x) / 2.0f,// 水平居中
            y + (bh - textBounds.size.y) / 2.0f - 2.0f// 垂直居中
        });
    }
};

// 图形基类
struct ShapeBase {
    virtual ~ShapeBase() = default;
    virtual void draw(sf::RenderWindow& window, float zoomLevel) const = 0;
};

// 圆形
struct CircleShape : ShapeBase {
    sf::Vector2f center;
    float radius;

    void draw(sf::RenderWindow& window, float zoomLevel) const override {
        sf::CircleShape shape(radius);
        shape.setPosition({center.x - radius, center.y - radius});
        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineColor(sf::Color(0, 200, 255));
        shape.setOutlineThickness(2.0f / zoomLevel);
        window.draw(shape);
    }
};

// 直线
struct LineShape : ShapeBase {
    sf::Vector2f start;
    sf::Vector2f end;

    void draw(sf::RenderWindow& window, float zoomLevel) const override {
        sf::VertexArray line(sf::PrimitiveType::Lines);
        sf::Vertex v1; v1.position = start; v1.color = sf::Color(255, 100, 100);
        sf::Vertex v2; v2.position = end;   v2.color = sf::Color(255, 100, 100);
        line.append(v1); line.append(v2);
        window.draw(line);

        sf::CircleShape dot1(4 / zoomLevel);
        dot1.setPosition({start.x - 4 / zoomLevel, start.y - 4 / zoomLevel});
        dot1.setFillColor(sf::Color(255, 100, 100));
        window.draw(dot1);

        sf::CircleShape dot2(4 / zoomLevel);
        dot2.setPosition({end.x - 4 / zoomLevel, end.y - 4 / zoomLevel});
        dot2.setFillColor(sf::Color(255, 100, 100));
        window.draw(dot2);
    }
};

// 矩形
struct RectShape : ShapeBase {
    sf::Vector2f topLeft;
    float width;
    float height;

    void draw(sf::RenderWindow& window, float zoomLevel) const override {
        sf::RectangleShape shape({std::abs(width), std::abs(height)});
        float posX = width >= 0 ? topLeft.x : topLeft.x + width;
        float posY = height >= 0 ? topLeft.y : topLeft.y + height;
        shape.setPosition({posX, posY});
        shape.setFillColor(sf::Color::Transparent);
        shape.setOutlineColor(sf::Color(100, 255, 100));
        shape.setOutlineThickness(2.0f / zoomLevel);
        window.draw(shape);
    }
};

// 辅助函数：计算三点确定的圆心和半径
bool calculateCircleFrom3Points(const sf::Vector2f& p1, const sf::Vector2f& p2, const sf::Vector2f& p3, 
                                 sf::Vector2f& outCenter, float& outRadius) {
    // 检查三点是否共线
    float d = 2.0f * (p1.x * (p2.y - p3.y) + p2.x * (p3.y - p1.y) + p3.x * (p1.y - p2.y));
    if (std::abs(d) < 0.001f) return false; // 三点共线或近似共线，无法确定圆

    float a = p1.x * p1.x + p1.y * p1.y;
    float b = p2.x * p2.x + p2.y * p2.y;
    float c = p3.x * p3.x + p3.y * p3.y;

    outCenter.x = (a * (p2.y - p3.y) + b * (p3.y - p1.y) + c * (p1.y - p2.y)) / d;
    outCenter.y = (a * (p3.x - p2.x) + b * (p1.x - p3.x) + c * (p2.x - p1.x)) / d;
    outRadius = std::hypot(p1.x - outCenter.x, p1.y - outCenter.y);

    return true;
}

int main() {
    sf::RenderWindow window(sf::VideoMode({1024, 768}), "Simple CAD - Multi Circle Modes | Line | Rectangle", sf::Style::Default);
    window.setFramerateLimit(60);// 设置最大帧率为60

    ToolType currentTool = ToolType::Circle;
    std::vector<std::unique_ptr<ShapeBase>> shapes;// 图形列表

    // 拖拽状态管理
    std::optional<sf::Vector2f> dragStart;      // 拖拽起点 / 三点画圆第一点
    std::optional<sf::Vector2f> currentMouse;   // 鼠标当前位置
    std::optional<sf::Vector2f> secondPoint;    // 三点画圆的第二个点
    int circleStep = 0;  // 三点画圆步骤：0=未开始, 1=已点第一点, 2=已点第二点

    sf::View view = window.getDefaultView();// 默认视图
    float zoomLevel = 1.0f;// 缩放级别，默认1.0f
    bool isPanning = false;// 是否正在平移
    sf::Vector2f panStartMouse;// 平移起点鼠标位置
    sf::Vector2f panStartCenter;// 平移起点中心位置 

    // 字体加载
    sf::Font font;
    bool fontLoaded = false;

    fontLoaded = font.openFromFile("C:\\Windows\\Fonts\\arial.ttf") || fontLoaded;
    fontLoaded = font.openFromFile("C:\\Windows\\Fonts\\segoeui.ttf") || fontLoaded;
    fontLoaded = font.openFromFile("C:\\Windows\\Fonts\\msyh.ttc") || fontLoaded;

    // 工具栏按钮
    std::vector<ToolButton> toolButtons;
    const float toolbarHeight = 50.0f;
    const float buttonWidth = 130.0f;
    const float buttonHeight = 36.0f;
    const float buttonSpacing = 8.0f;
    const float btnStartX = 20.0f;
    const float btnStartY = (toolbarHeight - buttonHeight) / 2.0f;

    toolButtons.emplace_back(btnStartX, btnStartY, "Circle 2Pt", 
        ToolType::Circle, true, font, buttonWidth, buttonHeight);
    toolButtons.emplace_back(btnStartX + buttonWidth + buttonSpacing, btnStartY, "Circle Center", 
        ToolType::CircleCenter, false, font, buttonWidth, buttonHeight);
    toolButtons.emplace_back(btnStartX + 2 * (buttonWidth + buttonSpacing), btnStartY, "Circle 3Pt", 
        ToolType::Circle3Point, false, font, buttonWidth, buttonHeight);
    toolButtons.emplace_back(btnStartX + 3 * (buttonWidth + buttonSpacing), btnStartY, "Line (L)", 
        ToolType::Line, false, font, buttonWidth, buttonHeight);
    toolButtons.emplace_back(btnStartX + 4 * (buttonWidth + buttonSpacing), btnStartY, "Rect (R)", 
        ToolType::Rectangle, false, font, buttonWidth, buttonHeight);

    // 信息文本
    sf::Text infoText(font);
    infoText.setCharacterSize(16);
    infoText.setFillColor(sf::Color::White);
    infoText.setPosition({10, toolbarHeight + 10});

    sf::Text measureLabel(font);
    measureLabel.setFillColor(sf::Color(0, 255, 100));

    // 工具栏背景
    sf::RectangleShape toolbarBg({(float)window.getSize().x, toolbarHeight});
    toolbarBg.setFillColor(sf::Color(40, 40, 45));
    toolbarBg.setOutlineColor(sf::Color(80, 80, 90));
    toolbarBg.setOutlineThickness(1.0f);

    // 提示文本（用于三点画圆步骤提示）
    sf::Text hintText(font);
    hintText.setCharacterSize(18);
    hintText.setFillColor(sf::Color(255, 200, 100));
    hintText.setPosition({10, toolbarHeight + 35});

    while (window.isOpen()) {
        sf::Vector2i mousePixelPos = sf::Mouse::getPosition(window);
        sf::Vector2f mouseScreenPos = sf::Vector2f(mousePixelPos);

        // 检测按钮悬停
        for (auto& btn : toolButtons) {
            bool wasHovered = btn.hovered;
            btn.hovered = btn.shape.getGlobalBounds().contains(mouseScreenPos);
            if (btn.hovered != wasHovered) {
                if (btn.selected) {
                    btn.shape.setFillColor(sf::Color(70, 140, 200));
                } else {
                    btn.shape.setFillColor(btn.hovered ? sf::Color(80, 80, 90) : sf::Color(60, 60, 60));
                }
            }
        }

        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();

            // 鼠标滚轮缩放
            if (auto* wheelScrolled = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (wheelScrolled->wheel == sf::Mouse::Wheel::Vertical) {
                    sf::Vector2f mouseWorldBefore = window.mapPixelToCoords(mousePixelPos, view);

                    float zoomFactor = (wheelScrolled->delta > 0) ? 0.9f : 1.1f;
                    zoomLevel *= zoomFactor;

                    if (zoomLevel < 0.1f) zoomLevel = 0.1f;
                    if (zoomLevel > 10.0f) zoomLevel = 10.0f;

                    view.setSize({window.getSize().x * zoomLevel, window.getSize().y * zoomLevel});

                    sf::Vector2f mouseWorldAfter = window.mapPixelToCoords(mousePixelPos, view);
                    view.move(mouseWorldBefore - mouseWorldAfter);
                }
            }

            // 鼠标按下
            if (auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                sf::Vector2f screenPos = sf::Vector2f(mousePressed->position);

                bool clickedToolbar = false;
                if (screenPos.y < toolbarHeight) {
                    for (auto& btn : toolButtons) {
                        if (btn.shape.getGlobalBounds().contains(screenPos)) {
                            for (auto& b : toolButtons) {
                                b.selected = false;
                                b.shape.setFillColor(sf::Color(60, 60, 60));
                                b.shape.setOutlineColor(sf::Color(100, 100, 100));
                            }
                            btn.selected = true;
                            btn.shape.setFillColor(sf::Color(60, 120, 180));
                            btn.shape.setOutlineColor(sf::Color(100, 180, 255));
                            currentTool = btn.tool;
                            clickedToolbar = true;
                            // 重置所有画圆状态
                            dragStart.reset();
                            currentMouse.reset();
                            secondPoint.reset();
                            circleStep = 0;
                            break;
                        }
                    }
                }

                if (!clickedToolbar) {
                    if (mousePressed->button == sf::Mouse::Button::Middle) {
                        isPanning = true;
                        panStartMouse = sf::Vector2f(mousePressed->position);
                        panStartCenter = view.getCenter();
                    }
                    else if (mousePressed->button == sf::Mouse::Button::Left && !isPanning) {
                        if (screenPos.y > toolbarHeight) {
                            sf::Vector2f worldPos = window.mapPixelToCoords(mousePressed->position, view);

                            if (currentTool == ToolType::Circle3Point) {
                                // 三点画圆逻辑
                                if (circleStep == 0) {
                                    // 第一点
                                    dragStart = worldPos;
                                    circleStep = 1;
                                } else if (circleStep == 1) {
                                    // 第二点
                                    secondPoint = worldPos;
                                    circleStep = 2;
                                } else if (circleStep == 2) {
                                    // 第三点，完成画圆
                                    sf::Vector2f center;
                                    float radius;
                                    if (calculateCircleFrom3Points(*dragStart, *secondPoint, worldPos, center, radius)) {
                                        if (radius > 1.0f) {
                                            auto circle = std::make_unique<CircleShape>();
                                            circle->center = center;
                                            circle->radius = radius;
                                            shapes.push_back(std::move(circle));
                                        }
                                    }
                                    // 重置所有状态
                                    dragStart.reset();
                                    secondPoint.reset();
                                    circleStep = 0;
                                    currentMouse.reset();
                                }
                            } else {
                                // 普通拖拽画圆/线/矩形
                                dragStart = worldPos;
                                currentMouse = worldPos;
                            }
                        }
                    }
                }
            }

            // 鼠标移动
            if (auto* mouseMoved = event->getIf<sf::Event::MouseMoved>()) {
                if (isPanning) {
                    sf::Vector2f currentPos = sf::Vector2f(mouseMoved->position);
                    sf::Vector2f delta = panStartMouse - currentPos;
                    delta.x *= zoomLevel;
                    delta.y *= zoomLevel;
                    view.setCenter(panStartCenter + delta);
                }
                else if (dragStart.has_value()) {
                    currentMouse = sf::Vector2f(window.mapPixelToCoords(mouseMoved->position, view));
                }
            }

            // 鼠标释放
            if (auto* mouseReleased = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (mouseReleased->button == sf::Mouse::Button::Middle) {
                    isPanning = false;
                }
                else if (mouseReleased->button == sf::Mouse::Button::Left && dragStart.has_value()) {
                    // 三点画圆：在鼠标释放时不做任何处理
                    // 三点画圆的所有逻辑都在 MouseButtonPressed 中完成
                    // 这里只需要忽略它，不要进入下面的 switch
                    if (currentTool == ToolType::Circle3Point) {
                        // 三点画圆已经在 press 时处理完毕，这里什么都不做
                        // 但如果 circleStep == 0（异常情况），重置一下
                        if (circleStep == 0) {
                            dragStart.reset();
                            currentMouse.reset();
                        }
                        continue; // 跳过下面的 switch，避免误处理
                    }

                    sf::Vector2f endPos = window.mapPixelToCoords(mouseReleased->position, view);

                    switch (currentTool) {
                        case ToolType::Circle: {
                            // 两点画圆（直径方式）
                            sf::Vector2f center = (*dragStart + endPos) / 2.0f;
                            float diameter = std::hypot(endPos.x - dragStart->x, endPos.y - dragStart->y);
                            float radius = diameter / 2.0f;
                            if (radius > 1.0f) {
                                auto circle = std::make_unique<CircleShape>();
                                circle->center = center;
                                circle->radius = radius;
                                shapes.push_back(std::move(circle));
                            }
                            break;
                        }
                        case ToolType::CircleCenter: {
                            // 中心+半径方式
                            float radius = std::hypot(endPos.x - dragStart->x, endPos.y - dragStart->y);
                            if (radius > 1.0f) {
                                auto circle = std::make_unique<CircleShape>();
                                circle->center = *dragStart;
                                circle->radius = radius;
                                shapes.push_back(std::move(circle));
                            }
                            break;
                        }
                        case ToolType::Line: {
                            float length = std::hypot(endPos.x - dragStart->x, endPos.y - dragStart->y);
                            if (length > 1.0f) {
                                auto line = std::make_unique<LineShape>();
                                line->start = *dragStart;
                                line->end = endPos;
                                shapes.push_back(std::move(line));
                            }
                            break;
                        }
                        case ToolType::Rectangle: {
                            float w = endPos.x - dragStart->x;
                            float h = endPos.y - dragStart->y;
                            if (std::abs(w) > 1.0f && std::abs(h) > 1.0f) {
                                auto rect = std::make_unique<RectShape>();
                                rect->topLeft = *dragStart;
                                rect->width = w;
                                rect->height = h;
                                shapes.push_back(std::move(rect));
                            }
                            break;
                        }
                        default:
                            break;
                    }
                    dragStart.reset();
                    currentMouse.reset();
                }
            }

            // 键盘快捷键
            if (auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                auto scancode = keyPressed->scancode;

                if (scancode == sf::Keyboard::Scan::C) {
                    shapes.clear();
                    dragStart.reset();
                    secondPoint.reset();
                    circleStep = 0;
                    currentMouse.reset();
                }
                else if (scancode == sf::Keyboard::Scan::V) {
                    zoomLevel = 1.0f;
                    view = window.getDefaultView();
                }
                else if (scancode == sf::Keyboard::Scan::O) {
                    for (auto& b : toolButtons) {
                        b.selected = (b.tool == ToolType::Circle);
                        b.shape.setFillColor(b.selected ? sf::Color(60, 120, 180) : sf::Color(60, 60, 60));
                        b.shape.setOutlineColor(b.selected ? sf::Color(100, 180, 255) : sf::Color(100, 100, 100));
                    }
                    currentTool = ToolType::Circle;
                    dragStart.reset();
                    secondPoint.reset();
                    circleStep = 0;
                    currentMouse.reset();
                }
                else if (scancode == sf::Keyboard::Scan::L) {
                    for (auto& b : toolButtons) {
                        b.selected = (b.tool == ToolType::Line);
                        b.shape.setFillColor(b.selected ? sf::Color(60, 120, 180) : sf::Color(60, 60, 60));
                        b.shape.setOutlineColor(b.selected ? sf::Color(100, 180, 255) : sf::Color(100, 100, 100));
                    }
                    currentTool = ToolType::Line;
                    dragStart.reset();
                    secondPoint.reset();
                    circleStep = 0;
                    currentMouse.reset();
                }
                else if (scancode == sf::Keyboard::Scan::R) {
                    for (auto& b : toolButtons) {
                        b.selected = (b.tool == ToolType::Rectangle);
                        b.shape.setFillColor(b.selected ? sf::Color(60, 120, 180) : sf::Color(60, 60, 60));
                        b.shape.setOutlineColor(b.selected ? sf::Color(100, 180, 255) : sf::Color(100, 100, 100));
                    }
                    currentTool = ToolType::Rectangle;
                    dragStart.reset();
                    secondPoint.reset();
                    circleStep = 0;
                    currentMouse.reset();
                }
                else if (scancode == sf::Keyboard::Scan::Num1) {
                    for (auto& b : toolButtons) {
                        b.selected = (b.tool == ToolType::Circle);
                        b.shape.setFillColor(b.selected ? sf::Color(60, 120, 180) : sf::Color(60, 60, 60));
                        b.shape.setOutlineColor(b.selected ? sf::Color(100, 180, 255) : sf::Color(100, 100, 100));
                    }
                    currentTool = ToolType::Circle;
                    dragStart.reset();
                    secondPoint.reset();
                    circleStep = 0;
                    currentMouse.reset();
                }
                else if (scancode == sf::Keyboard::Scan::Num2) {
                    for (auto& b : toolButtons) {
                        b.selected = (b.tool == ToolType::CircleCenter);
                        b.shape.setFillColor(b.selected ? sf::Color(60, 120, 180) : sf::Color(60, 60, 60));
                        b.shape.setOutlineColor(b.selected ? sf::Color(100, 180, 255) : sf::Color(100, 100, 100));
                    }
                    currentTool = ToolType::CircleCenter;
                    dragStart.reset();
                    secondPoint.reset();
                    circleStep = 0;
                    currentMouse.reset();
                }
                else if (scancode == sf::Keyboard::Scan::Num3) {
                    for (auto& b : toolButtons) {
                        b.selected = (b.tool == ToolType::Circle3Point);
                        b.shape.setFillColor(b.selected ? sf::Color(60, 120, 180) : sf::Color(60, 60, 60));
                        b.shape.setOutlineColor(b.selected ? sf::Color(100, 180, 255) : sf::Color(100, 100, 100));
                    }
                    currentTool = ToolType::Circle3Point;
                    dragStart.reset();
                    secondPoint.reset();
                    circleStep = 0;
                    currentMouse.reset();
                }
                else if (scancode == sf::Keyboard::Scan::Escape) {
                    // ESC 取消当前画圆操作
                    dragStart.reset();
                    secondPoint.reset();
                    circleStep = 0;
                    currentMouse.reset();
                }
            }

            if (event->is<sf::Event::Resized>()) {
                toolbarBg.setSize({(float)window.getSize().x, toolbarHeight});
            }
        }

        // 渲染
        window.clear(sf::Color(30, 30, 30));
        window.setView(view);

        // 绘制网格
        sf::Color gridColor(50, 50, 50);
        float gridSize = 50.0f;
        sf::Vector2f viewCenter = view.getCenter();
        sf::Vector2f viewSize = view.getSize();

        float startX_grid = std::floor((viewCenter.x - viewSize.x / 2) / gridSize) * gridSize;
        float endX_grid = std::ceil((viewCenter.x + viewSize.x / 2) / gridSize) * gridSize;
        float startY_grid = std::floor((viewCenter.y - viewSize.y / 2) / gridSize) * gridSize;
        float endY_grid = std::ceil((viewCenter.y + viewSize.y / 2) / gridSize) * gridSize;

        sf::VertexArray gridLines(sf::PrimitiveType::Lines);
        for (float x = startX_grid; x <= endX_grid; x += gridSize) {
            sf::Vertex v1; v1.position = {x, startY_grid}; v1.color = gridColor;
            sf::Vertex v2; v2.position = {x, endY_grid}; v2.color = gridColor;
            gridLines.append(v1); gridLines.append(v2);
        }
        for (float y = startY_grid; y <= endY_grid; y += gridSize) {
            sf::Vertex v1; v1.position = {startX_grid, y}; v1.color = gridColor;
            sf::Vertex v2; v2.position = {endX_grid, y}; v2.color = gridColor;
            gridLines.append(v1); gridLines.append(v2);
        }
        window.draw(gridLines);

        // 绘制坐标轴
        sf::VertexArray axes(sf::PrimitiveType::Lines);
        sf::Vertex ax1; ax1.position = {-10000, 0}; ax1.color = sf::Color(100, 100, 100);
        sf::Vertex ax2; ax2.position = {10000, 0}; ax2.color = sf::Color(100, 100, 100);
        sf::Vertex ay1; ay1.position = {0, -10000}; ay1.color = sf::Color(100, 100, 100);
        sf::Vertex ay2; ay2.position = {0, 10000}; ay2.color = sf::Color(100, 100, 100);
        axes.append(ax1); axes.append(ax2);
        axes.append(ay1); axes.append(ay2);
        window.draw(axes);

        // 绘制所有已完成的图形
        for (const auto& shape : shapes) {
            shape->draw(window, zoomLevel);
        }

        // 绘制预览和辅助点
        if (currentTool == ToolType::Circle3Point && circleStep > 0) {
            // 绘制已确定的点
            if (dragStart.has_value()) {
                sf::CircleShape dot1(5 / zoomLevel);
                dot1.setPosition({dragStart->x - 5 / zoomLevel, dragStart->y - 5 / zoomLevel});
                dot1.setFillColor(sf::Color(255, 200, 50));
                window.draw(dot1);
            }
            if (secondPoint.has_value()) {
                sf::CircleShape dot2(5 / zoomLevel);
                dot2.setPosition({secondPoint->x - 5 / zoomLevel, secondPoint->y - 5 / zoomLevel});
                dot2.setFillColor(sf::Color(255, 200, 50));
                window.draw(dot2);

                // 绘制两点之间的连线
                sf::VertexArray line(sf::PrimitiveType::Lines);
                sf::Vertex v1; v1.position = *dragStart; v1.color = sf::Color(255, 200, 50, 150);
                sf::Vertex v2; v2.position = *secondPoint; v2.color = sf::Color(255, 200, 50, 150);
                line.append(v1); line.append(v2);
                window.draw(line);
            }
            if (currentMouse.has_value() && circleStep == 2) {
                // 绘制三点预览圆
                sf::Vector2f previewCenter;
                float previewRadius;
                if (calculateCircleFrom3Points(*dragStart, *secondPoint, *currentMouse, previewCenter, previewRadius)) {
                    sf::CircleShape preview(previewRadius);
                    preview.setPosition({previewCenter.x - previewRadius, previewCenter.y - previewRadius});
                    preview.setFillColor(sf::Color(100, 150, 255, 80));
                    preview.setOutlineColor(sf::Color(255, 200, 100));
                    preview.setOutlineThickness(2.0f / zoomLevel);
                    window.draw(preview);

                    // 绘制圆心
                    sf::CircleShape centerDot(4 / zoomLevel);
                    centerDot.setPosition({previewCenter.x - 4 / zoomLevel, previewCenter.y - 4 / zoomLevel});
                    centerDot.setFillColor(sf::Color::Red);
                    window.draw(centerDot);

                    // 显示半径
                    if (fontLoaded && previewRadius > 5.0f) {
                        std::string radiusStr = "r=" + std::to_string((int)previewRadius);
                        measureLabel.setString(radiusStr);
                        measureLabel.setCharacterSize(std::max(10, (int)(14 / zoomLevel)));
                        measureLabel.setPosition({previewCenter.x + 5 / zoomLevel, previewCenter.y - 5 / zoomLevel});
                        window.draw(measureLabel);
                    }
                }

                // 绘制当前鼠标位置的点
                sf::CircleShape dot3(5 / zoomLevel);
                dot3.setPosition({currentMouse->x - 5 / zoomLevel, currentMouse->y - 5 / zoomLevel});
                dot3.setFillColor(sf::Color(255, 200, 50));
                window.draw(dot3);
            }
        }
        else if (dragStart.has_value() && currentMouse.has_value()) {
            switch (currentTool) {
                case ToolType::Circle: {
                    // 两点画圆预览（直径方式）
                    sf::Vector2f center = (*dragStart + *currentMouse) / 2.0f;
                    float diameter = std::hypot(currentMouse->x - dragStart->x, currentMouse->y - dragStart->y);
                    float radius = diameter / 2.0f;

                    sf::CircleShape preview(radius);
                    preview.setPosition({center.x - radius, center.y - radius});
                    preview.setFillColor(sf::Color(100, 150, 255, 80));
                    preview.setOutlineColor(sf::Color(255, 200, 100));
                    preview.setOutlineThickness(2.0f / zoomLevel);
                    window.draw(preview);

                    sf::VertexArray radiusLine(sf::PrimitiveType::Lines);
                    sf::Vertex r1; r1.position = center; r1.color = sf::Color(0, 255, 100, 220);
                    sf::Vertex r2; r2.position = *currentMouse; r2.color = sf::Color(0, 255, 100, 220);
                    radiusLine.append(r1); radiusLine.append(r2);
                    window.draw(radiusLine);

                    sf::CircleShape centerDot(4 / zoomLevel);
                    centerDot.setPosition({center.x - 4 / zoomLevel, center.y - 4 / zoomLevel});
                    centerDot.setFillColor(sf::Color::Red);
                    window.draw(centerDot);

                    sf::CircleShape endDot(3 / zoomLevel);
                    endDot.setPosition({currentMouse->x - 3 / zoomLevel, currentMouse->y - 3 / zoomLevel});
                    endDot.setFillColor(sf::Color(255, 200, 100));
                    window.draw(endDot);

                    if (fontLoaded && radius > 5.0f) {
                        std::string radiusStr = "r=" + std::to_string((int)radius);
                        measureLabel.setString(radiusStr);
                        measureLabel.setCharacterSize(std::max(10, (int)(14 / zoomLevel)));
                        sf::Vector2f midPoint = (center + *currentMouse) / 2.0f;
                        measureLabel.setPosition({midPoint.x + 5 / zoomLevel, midPoint.y - 5 / zoomLevel});
                        window.draw(measureLabel);
                    }
                    break;
                }

                case ToolType::CircleCenter: {
                    // 中心+半径方式预览
                    float radius = std::hypot(currentMouse->x - dragStart->x, currentMouse->y - dragStart->y);

                    sf::CircleShape preview(radius);
                    preview.setPosition({dragStart->x - radius, dragStart->y - radius});
                    preview.setFillColor(sf::Color(100, 150, 255, 80));
                    preview.setOutlineColor(sf::Color(255, 200, 100));
                    preview.setOutlineThickness(2.0f / zoomLevel);
                    window.draw(preview);

                    // 绘制中心点
                    sf::CircleShape centerDot(4 / zoomLevel);
                    centerDot.setPosition({dragStart->x - 4 / zoomLevel, dragStart->y - 4 / zoomLevel});
                    centerDot.setFillColor(sf::Color::Red);
                    window.draw(centerDot);

                    // 绘制半径线
                    sf::VertexArray radiusLine(sf::PrimitiveType::Lines);
                    sf::Vertex r1; r1.position = *dragStart; r1.color = sf::Color(0, 255, 100, 220);
                    sf::Vertex r2; r2.position = *currentMouse; r2.color = sf::Color(0, 255, 100, 220);
                    radiusLine.append(r1); radiusLine.append(r2);
                    window.draw(radiusLine);

                    // 绘制鼠标端点
                    sf::CircleShape endDot(3 / zoomLevel);
                    endDot.setPosition({currentMouse->x - 3 / zoomLevel, currentMouse->y - 3 / zoomLevel});
                    endDot.setFillColor(sf::Color(255, 200, 100));
                    window.draw(endDot);

                    if (fontLoaded && radius > 5.0f) {
                        std::string radiusStr = "r=" + std::to_string((int)radius);
                        measureLabel.setString(radiusStr);
                        measureLabel.setCharacterSize(std::max(10, (int)(14 / zoomLevel)));
                        sf::Vector2f midPoint = (*dragStart + *currentMouse) / 2.0f;
                        measureLabel.setPosition({midPoint.x + 5 / zoomLevel, midPoint.y - 5 / zoomLevel});
                        window.draw(measureLabel);
                    }
                    break;
                }

                case ToolType::Line: {
                    sf::VertexArray previewLine(sf::PrimitiveType::Lines);
                    sf::Vertex v1; v1.position = *dragStart; v1.color = sf::Color(255, 100, 100, 180);
                    sf::Vertex v2; v2.position = *currentMouse; v2.color = sf::Color(255, 100, 100, 180);
                    previewLine.append(v1); previewLine.append(v2);
                    window.draw(previewLine);

                    sf::CircleShape dot1(4 / zoomLevel);
                    dot1.setPosition({dragStart->x - 4 / zoomLevel, dragStart->y - 4 / zoomLevel});
                    dot1.setFillColor(sf::Color(255, 100, 100));
                    window.draw(dot1);

                    sf::CircleShape dot2(4 / zoomLevel);
                    dot2.setPosition({currentMouse->x - 4 / zoomLevel, currentMouse->y - 4 / zoomLevel});
                    dot2.setFillColor(sf::Color(255, 100, 100));
                    window.draw(dot2);

                    float length = std::hypot(currentMouse->x - dragStart->x, currentMouse->y - dragStart->y);
                    if (fontLoaded && length > 5.0f) {
                        std::string lenStr = "L=" + std::to_string((int)length);
                        measureLabel.setString(lenStr);
                        measureLabel.setCharacterSize(std::max(10, (int)(14 / zoomLevel)));
                        sf::Vector2f midPoint = (*dragStart + *currentMouse) / 2.0f;
                        measureLabel.setPosition({midPoint.x + 5 / zoomLevel, midPoint.y - 5 / zoomLevel});
                        window.draw(measureLabel);
                    }
                    break;
                }

                case ToolType::Rectangle: {
                    float w = currentMouse->x - dragStart->x;
                    float h = currentMouse->y - dragStart->y;

                    sf::RectangleShape preview({std::abs(w), std::abs(h)});
                    float posX = w >= 0 ? dragStart->x : dragStart->x + w;
                    float posY = h >= 0 ? dragStart->y : dragStart->y + h;
                    preview.setPosition({posX, posY});
                    preview.setFillColor(sf::Color(100, 255, 100, 60));
                    preview.setOutlineColor(sf::Color(100, 255, 100, 200));
                    preview.setOutlineThickness(2.0f / zoomLevel);
                    window.draw(preview);

                    sf::CircleShape cornerDot(3 / zoomLevel);
                    cornerDot.setFillColor(sf::Color(100, 255, 100));

                    sf::Vector2f corners[4] = {
                        *dragStart,
                        {dragStart->x + w, dragStart->y},
                        *currentMouse,
                        {dragStart->x, dragStart->y + h}
                    };
                    for (const auto& corner : corners) {
                        cornerDot.setPosition({corner.x - 3 / zoomLevel, corner.y - 3 / zoomLevel});
                        window.draw(cornerDot);
                    }

                    if (fontLoaded && std::abs(w) > 5.0f && std::abs(h) > 5.0f) {
                        std::string sizeStr = std::to_string((int)std::abs(w)) + " x " + std::to_string((int)std::abs(h));
                        measureLabel.setString(sizeStr);
                        measureLabel.setCharacterSize(std::max(10, (int)(14 / zoomLevel)));
                        sf::Vector2f center = (*dragStart + *currentMouse) / 2.0f;
                        measureLabel.setPosition({center.x + 5 / zoomLevel, center.y - 5 / zoomLevel});
                        window.draw(measureLabel);
                    }
                    break;
                }

                default:
                    break;
            }
        }

        // 绘制UI
        window.setView(window.getDefaultView());

        window.draw(toolbarBg);

        for (auto& btn : toolButtons) {
            window.draw(btn.shape);
            window.draw(btn.label);
        }

        sf::Vector2f mouseWorld = window.mapPixelToCoords(mousePixelPos, view);
        std::string info;
        std::string toolName;
        switch (currentTool) {
            case ToolType::Circle: toolName = "Circle (2-Point/Diameter)"; break;
            case ToolType::CircleCenter: toolName = "Circle (Center+Radius)"; break;
            case ToolType::Circle3Point: toolName = "Circle (3-Point)"; break;
            case ToolType::Line: toolName = "Line"; break;
            case ToolType::Rectangle: toolName = "Rectangle"; break;
        }

        if (currentTool == ToolType::Circle3Point && circleStep > 0) {
            // 三点画圆状态提示
            if (circleStep == 1) {
                info = "[3-Point Circle] Step 1/3: First point set. Click second point.";
            } else if (circleStep == 2) {
                info = "[3-Point Circle] Step 2/3: Second point set. Move mouse to preview, click third point.";
            }
            info += "  ESC: cancel";
        }
        else if (dragStart.has_value() && currentMouse.has_value() && currentTool != ToolType::Circle3Point) {
            switch (currentTool) {
                case ToolType::Circle: {
                    sf::Vector2f c = (*dragStart + *currentMouse) / 2.0f;
                    float r = std::hypot(currentMouse->x - dragStart->x, currentMouse->y - dragStart->y) / 2.0f;
                    info = "[Circle 2Pt] Center: (" + std::to_string((int)c.x) + ", " + std::to_string((int)c.y) +
                           ")  Radius: " + std::to_string((int)r);
                    break;
                }
                case ToolType::CircleCenter: {
                    float r = std::hypot(currentMouse->x - dragStart->x, currentMouse->y - dragStart->y);
                    info = "[Circle Center] Center: (" + std::to_string((int)dragStart->x) + ", " + std::to_string((int)dragStart->y) +
                           ")  Radius: " + std::to_string((int)r);
                    break;
                }
                case ToolType::Line: {
                    float len = std::hypot(currentMouse->x - dragStart->x, currentMouse->y - dragStart->y);
                    info = "[Line] Length: " + std::to_string((int)len);
                    break;
                }
                case ToolType::Rectangle: {
                    float w = currentMouse->x - dragStart->x;
                    float h = currentMouse->y - dragStart->y;
                    info = "[Rectangle] " + std::to_string((int)std::abs(w)) + " x " + std::to_string((int)std::abs(h));
                    break;
                }
                default:
                    break;
            }
            info += "  Zoom: " + std::to_string((int)(100.0f/zoomLevel)) + "%";
        } else {
            info = "Tool: " + toolName + " | Left drag: draw | Wheel: zoom | Mid drag: pan | C: clear | V: reset view | 1/2/3/O/L/R: switch tool | ESC: cancel\n";
            info += "Mouse: (" + std::to_string((int)mouseWorld.x) + ", " + std::to_string((int)mouseWorld.y) + ")";
            info += "  Shapes: " + std::to_string(shapes.size());
            info += "  Zoom: " + std::to_string((int)(100.0f/zoomLevel)) + "%";
        }
        infoText.setString(info);
        window.draw(infoText);

        // 三点画圆步骤提示
        if (currentTool == ToolType::Circle3Point) {
            std::string hint;
            if (circleStep == 0) {
                hint = "Click to set first point";
            } else if (circleStep == 1) {
                hint = "Click to set second point";
            } else if (circleStep == 2) {
                hint = "Click to set third point - Preview shown";
            }
            hintText.setString(hint);
            window.draw(hintText);
        }

        window.display();
    }
    return 0;
}
