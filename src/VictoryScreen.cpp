#include "VictoryScreen.h"
#include "FontManager.h"
#include "Constants.h"
#include "Types.h"

VictoryScreen::VictoryScreen(bool isVictory, int localPlayerSlot, const GameStatistics& stats)
    : m_isVictory(isVictory)
    , m_localPlayerSlot(localPlayerSlot)
    , m_stats(stats)
{
    m_font = FontManager::instance().defaultFont();
    
    // Create title text
    std::string titleText = isVictory ? "VICTORY!" : "DEFEAT";
    m_title.emplace(*m_font, titleText, 56);
    m_title->setFillColor(isVictory ? sf::Color(0x2E, 0xCC, 0x71) : sf::Color(0xE7, 0x4C, 0x3C));
    m_title->setStyle(sf::Text::Bold);
    
    // Build the statistics table
    buildStatsTable();
    
    // Initial layout
    sf::Vector2u defaultSize(Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT);
    layoutUI(defaultSize);
}

void VictoryScreen::buildStatsTable() {
    // Table headers
    std::vector<std::string> headers = { "Player", "Units Created", "Units Destroyed", "Buildings Created", "Buildings Destroyed" };
    
    m_tableHeaders.clear();
    for (const auto& header : headers) {
        sf::Text text(*m_font, header, 18);
        text.setFillColor(sf::Color(200, 200, 200));
        text.setStyle(sf::Text::Bold);
        m_tableHeaders.push_back(text);
    }
    
    // Table rows for each active player
    m_tableRows.clear();
    for (int slot = 0; slot < GameStatistics::MAX_PLAYERS; ++slot) {
        const auto& stats = m_stats.getStats(slot);
        if (!stats.isActive) continue;
        
        std::vector<sf::Text> row;
        
        // Player name
        sf::Text nameText(*m_font, getPlayerName(slot), 16);
        nameText.setFillColor(getPlayerColor(slot));
        row.push_back(nameText);
        
        // Stats columns
        sf::Text unitsCreated(*m_font, std::to_string(stats.unitsCreated), 16);
        unitsCreated.setFillColor(sf::Color::White);
        row.push_back(unitsCreated);
        
        sf::Text unitsDestroyed(*m_font, std::to_string(stats.unitsDestroyed), 16);
        unitsDestroyed.setFillColor(sf::Color::White);
        row.push_back(unitsDestroyed);
        
        sf::Text buildingsCreated(*m_font, std::to_string(stats.buildingsCreated), 16);
        buildingsCreated.setFillColor(sf::Color::White);
        row.push_back(buildingsCreated);
        
        sf::Text buildingsDestroyed(*m_font, std::to_string(stats.buildingsDestroyed), 16);
        buildingsDestroyed.setFillColor(sf::Color::White);
        row.push_back(buildingsDestroyed);
        
        m_tableRows.push_back(row);
    }
}

void VictoryScreen::layoutUI(sf::Vector2u windowSize) {
    m_lastWinSize = windowSize;
    
    float centerX = windowSize.x / 2.0f;
    float centerY = windowSize.y / 2.0f;
    
    // Center title near top
    sf::FloatRect titleBounds = m_title->getLocalBounds();
    m_title->setOrigin(sf::Vector2f(
        titleBounds.position.x + titleBounds.size.x / 2.0f,
        titleBounds.position.y + titleBounds.size.y / 2.0f
    ));
    m_title->setPosition(sf::Vector2f(centerX, 80.0f));
    
    // Statistics table layout
    float tableWidth = 700.0f;
    float rowHeight = 35.0f;
    float headerHeight = 40.0f;
    float colWidth = tableWidth / 5.0f;
    float tableStartX = centerX - tableWidth / 2.0f;
    float tableStartY = 150.0f;
    
    // Table background
    float tableHeight = headerHeight + rowHeight * m_tableRows.size() + 20.0f;
    m_tableBackground.setSize(sf::Vector2f(tableWidth + 20.0f, tableHeight));
    m_tableBackground.setPosition(sf::Vector2f(tableStartX - 10.0f, tableStartY - 10.0f));
    m_tableBackground.setFillColor(sf::Color(40, 40, 50, 220));
    m_tableBackground.setOutlineColor(sf::Color(80, 80, 100));
    m_tableBackground.setOutlineThickness(2.0f);
    
    // Position headers
    for (size_t i = 0; i < m_tableHeaders.size(); ++i) {
        float xPos = tableStartX + i * colWidth + colWidth / 2.0f;
        sf::FloatRect bounds = m_tableHeaders[i].getLocalBounds();
        m_tableHeaders[i].setOrigin(sf::Vector2f(
            bounds.position.x + bounds.size.x / 2.0f,
            bounds.position.y + bounds.size.y / 2.0f
        ));
        m_tableHeaders[i].setPosition(sf::Vector2f(xPos, tableStartY + headerHeight / 2.0f));
    }
    
    // Position rows
    for (size_t row = 0; row < m_tableRows.size(); ++row) {
        float yPos = tableStartY + headerHeight + row * rowHeight + rowHeight / 2.0f;
        for (size_t col = 0; col < m_tableRows[row].size(); ++col) {
            float xPos = tableStartX + col * colWidth + colWidth / 2.0f;
            sf::FloatRect bounds = m_tableRows[row][col].getLocalBounds();
            m_tableRows[row][col].setOrigin(sf::Vector2f(
                bounds.position.x + bounds.size.x / 2.0f,
                bounds.position.y + bounds.size.y / 2.0f
            ));
            m_tableRows[row][col].setPosition(sf::Vector2f(xPos, yPos));
        }
    }
    
    // Continue button at bottom
    float buttonW = 280.0f;
    float buttonH = 50.0f;
    float buttonY = windowSize.y - 100.0f;
    
    m_continueButton = createButton("Continue to Menu", 
        sf::Vector2f(centerX - buttonW / 2.0f, buttonY),
        sf::Vector2f(buttonW, buttonH));
}

VictoryScreen::Button VictoryScreen::createButton(const std::string& text, sf::Vector2f position, sf::Vector2f size) {
    Button button;
    
    button.shape.setSize(size);
    button.shape.setPosition(position);
    button.shape.setFillColor(sf::Color(60, 60, 60));
    button.shape.setOutlineColor(sf::Color(100, 100, 100));
    button.shape.setOutlineThickness(2.0f);
    
    button.label.emplace(*m_font, text, 22);
    button.label->setFillColor(sf::Color::White);
    
    // Center text in button
    sf::FloatRect textBounds = button.label->getLocalBounds();
    button.label->setOrigin(sf::Vector2f(
        textBounds.position.x + textBounds.size.x / 2.0f,
        textBounds.position.y + textBounds.size.y / 2.0f
    ));
    button.label->setPosition(sf::Vector2f(
        position.x + size.x / 2.0f,
        position.y + size.y / 2.0f
    ));
    
    return button;
}

bool VictoryScreen::isMouseOver(const Button& button, sf::Vector2f mousePos) const {
    sf::FloatRect bounds(button.shape.getPosition(), button.shape.getSize());
    return bounds.contains(mousePos);
}

void VictoryScreen::updateButtonHover(Button& button, sf::Vector2f mousePos) {
    button.hovered = isMouseOver(button, mousePos);
    
    if (button.hovered) {
        button.shape.setFillColor(sf::Color(80, 80, 100));
        button.shape.setOutlineColor(sf::Color(150, 150, 200));
    } else {
        button.shape.setFillColor(sf::Color(60, 60, 60));
        button.shape.setOutlineColor(sf::Color(100, 100, 100));
    }
}

std::string VictoryScreen::getPlayerName(int slot) const {
    if (slot == m_localPlayerSlot) {
        return "You (Player " + std::to_string(slot + 1) + ")";
    }
    return "Player " + std::to_string(slot + 1);
}

sf::Color VictoryScreen::getPlayerColor(int slot) const {
    return teamColor(teamFromIndex(slot));
}

ScreenResult VictoryScreen::handleEvent(const sf::Event& event) {
    // Handle mouse click
    if (const auto* mousePressed = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mousePressed->button == sf::Mouse::Button::Left) {
            sf::Vector2f mousePos(static_cast<float>(mousePressed->position.x),
                                  static_cast<float>(mousePressed->position.y));
            
            if (isMouseOver(m_continueButton, mousePos)) {
                return { ScreenResult::Action::BackToMenu, "" };
            }
        }
    }
    
    // Handle mouse movement for hover effects
    if (const auto* mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
        sf::Vector2f mousePos(static_cast<float>(mouseMoved->position.x),
                              static_cast<float>(mouseMoved->position.y));
        updateButtonHover(m_continueButton, mousePos);
    }
    
    // Handle window resize
    if (const auto* resized = event.getIf<sf::Event::Resized>()) {
        layoutUI(sf::Vector2u(resized->size.x, resized->size.y));
    }
    
    // ESC or Enter to continue
    if (const auto* keyPressed = event.getIf<sf::Event::KeyPressed>()) {
        if (keyPressed->code == sf::Keyboard::Key::Escape ||
            keyPressed->code == sf::Keyboard::Key::Enter) {
            return { ScreenResult::Action::BackToMenu, "" };
        }
    }
    
    return {};
}

ScreenResult VictoryScreen::update(float /*deltaTime*/) {
    return {};
}

void VictoryScreen::render(sf::RenderWindow& window) {
    // Handle window resize
    sf::Vector2u currentSize = window.getSize();
    if (currentSize != m_lastWinSize) {
        layoutUI(currentSize);
    }
    
    // Create a view matching the current window size for proper UI rendering
    sf::View uiView(sf::FloatRect({0.f, 0.f}, {static_cast<float>(currentSize.x), static_cast<float>(currentSize.y)}));
    window.setView(uiView);
    
    // Draw dark background overlay
    sf::RectangleShape background(sf::Vector2f(static_cast<float>(currentSize.x), 
                                                static_cast<float>(currentSize.y)));
    background.setFillColor(sf::Color(20, 20, 30));
    window.draw(background);
    
    // Draw title
    if (m_title.has_value()) {
        window.draw(*m_title);
    }
    
    // Draw table background
    window.draw(m_tableBackground);
    
    // Draw table headers
    for (const auto& header : m_tableHeaders) {
        window.draw(header);
    }
    
    // Draw table rows
    for (const auto& row : m_tableRows) {
        for (const auto& cell : row) {
            window.draw(cell);
        }
    }
    
    // Draw continue button
    window.draw(m_continueButton.shape);
    if (m_continueButton.label.has_value()) {
        window.draw(*m_continueButton.label);
    }
}
