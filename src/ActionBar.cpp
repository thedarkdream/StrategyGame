#include "ActionBar.h"
#include "Player.h"
#include "Unit.h"
#include "Building.h"
#include "Constants.h"

ActionBar::ActionBar() {
}

float ActionBar::getPanelX() const {
    return (Constants::WINDOW_WIDTH - Constants::ACTION_BAR_WIDTH) / 2.0f;
}

float ActionBar::getPanelY() const {
    return Constants::WINDOW_HEIGHT - Constants::ACTION_BAR_HEIGHT - 10.0f;
}

float ActionBar::getButtonY() const {
    return getPanelY() + Constants::ACTION_BAR_PADDING;
}

float ActionBar::getQueueY() const {
    return getButtonY() + Constants::ACTION_BAR_BUTTON_SIZE + 10.0f;
}

bool ActionBar::isPositionOnPanel(sf::Vector2i screenPos, Player& player) const {
    if (!player.getFirstOwnedSelectedEntity()) return false;
    
    float panelX = getPanelX();
    float panelY = getPanelY();
    
    return screenPos.x >= panelX && screenPos.x <= panelX + Constants::ACTION_BAR_WIDTH &&
           screenPos.y >= panelY && screenPos.y <= panelY + Constants::ACTION_BAR_HEIGHT;
}

int ActionBar::getButtonAtPosition(sf::Vector2i screenPos, EntityPtr entity) const {
    if (!entity) return -1;
    
    float panelX = getPanelX();
    float buttonY = getButtonY();
    
    // Check if within button row
    if (screenPos.y < buttonY || screenPos.y > buttonY + Constants::ACTION_BAR_BUTTON_SIZE) return -1;
    
    // Get action count from registry
    const auto& actions = ENTITY_DATA.getActions(entity->getType());
    int actionCount = static_cast<int>(actions.size());
    if (actionCount == 0) return -1;
    
    // Check each button
    float buttonX = panelX + Constants::ACTION_BAR_PADDING;
    for (int i = 0; i < actionCount; ++i) {
        if (screenPos.x >= buttonX && screenPos.x <= buttonX + Constants::ACTION_BAR_BUTTON_SIZE) {
            return i;
        }
        buttonX += Constants::ACTION_BAR_BUTTON_SIZE + Constants::ACTION_BAR_BUTTON_SPACING;
    }
    
    return -1;
}

int ActionBar::getQueueItemAtPosition(sf::Vector2i screenPos, EntityPtr entity) const {
    if (!entity) return -1;
    
    Building* building = dynamic_cast<Building*>(entity.get());
    if (!building || !building->isProducing()) return -1;
    
    std::vector<EntityType> queue = building->getProductionQueue();
    if (queue.empty()) return -1;
    
    float panelX = getPanelX();
    float queueY = getQueueY();
    float queueX = panelX + Constants::ACTION_BAR_PADDING;
    
    // Check main icon (index 0)
    if (screenPos.x >= queueX && screenPos.x <= queueX + MAIN_ICON_SIZE &&
        screenPos.y >= queueY && screenPos.y <= queueY + MAIN_ICON_SIZE) {
        return 0;
    }
    
    // Check queued icons (indices 1+)
    float queuedX = queueX + MAIN_ICON_SIZE + ICON_SPACING * 2;
    float queuedY = queueY + (MAIN_ICON_SIZE - QUEUED_ICON_SIZE) / 2.0f;
    
    for (size_t i = 1; i < queue.size() && i < 6; ++i) {
        if (screenPos.x >= queuedX && screenPos.x <= queuedX + QUEUED_ICON_SIZE &&
            screenPos.y >= queuedY && screenPos.y <= queuedY + QUEUED_ICON_SIZE) {
            return static_cast<int>(i);
        }
        queuedX += QUEUED_ICON_SIZE + ICON_SPACING;
    }
    
    return -1;
}

ActionBarClickResult ActionBar::handleClick(sf::Vector2i screenPos, Player& player) {
    ActionBarClickResult result;
    
    if (!isPositionOnPanel(screenPos, player)) return result;
    
    EntityPtr entity = player.getFirstOwnedSelectedEntity();
    if (!entity) return result;
    
    Building* building = dynamic_cast<Building*>(entity.get());
    Unit* unit = dynamic_cast<Unit*>(entity.get());
    
    // Check if clicking a queue item
    int queueIndex = getQueueItemAtPosition(screenPos, entity);
    if (queueIndex >= 0 && building) {
        building->cancelProductionAtIndex(queueIndex);
        result.type = ActionBarClickResult::Type::Handled;
        return result;
    }
    
    // Check if clicking an action button
    int buttonIndex = getButtonAtPosition(screenPos, entity);
    if (buttonIndex < 0) {
        result.type = ActionBarClickResult::Type::Handled;  // Clicked panel but not a button
        return result;
    }
    
    // Get action from registry
    const auto& actions = ENTITY_DATA.getActions(entity->getType());
    if (buttonIndex >= static_cast<int>(actions.size())) {
        result.type = ActionBarClickResult::Type::Handled;
        return result;
    }
    
    const ActionDef& action = actions[buttonIndex];
    
    switch (action.type) {
        case ActionDef::Type::TargetMove:
            result.type = ActionBarClickResult::Type::TargetMove;
            break;
            
        case ActionDef::Type::Instant:
            // Stop action - apply to all selected units
            for (auto& e : player.getSelection()) {
                if (auto* u = dynamic_cast<Unit*>(e.get())) {
                    u->stop();
                }
            }
            result.type = ActionBarClickResult::Type::Handled;
            break;
            
        case ActionDef::Type::TargetAttack:
            result.type = ActionBarClickResult::Type::TargetAttack;
            break;
            
        case ActionDef::Type::TargetGather:
            result.type = ActionBarClickResult::Type::TargetGather;
            break;
            
        case ActionDef::Type::Train:
            if (building && action.producesType != EntityType::None) {
                int mineralCost = ENTITY_DATA.getMineralCost(action.producesType);
                int gasCost = ENTITY_DATA.getGasCost(action.producesType);
                if (player.canAfford(mineralCost, gasCost)) {
                    if (building->trainUnit(action.producesType)) {
                        player.spendResources(mineralCost, gasCost);
                    }
                }
            }
            result.type = ActionBarClickResult::Type::Handled;
            break;
            
        case ActionDef::Type::Build:
            result.type = ActionBarClickResult::Type::TargetBuild;
            result.buildType = action.producesType;
            break;
    }
    
    return result;
}

void ActionBar::render(sf::RenderWindow& window, Player& player) {
    EntityPtr entity = player.getFirstOwnedSelectedEntity();
    if (!entity) return;
    
    float panelX = getPanelX();
    float panelY = getPanelY();
    
    // Draw panel background
    sf::RectangleShape bg(sf::Vector2f(Constants::ACTION_BAR_WIDTH, Constants::ACTION_BAR_HEIGHT));
    bg.setPosition(sf::Vector2f(panelX, panelY));
    bg.setFillColor(sf::Color(30, 30, 40, 230));
    bg.setOutlineThickness(2.0f);
    bg.setOutlineColor(sf::Color(80, 80, 100));
    window.draw(bg);
    
    if (!m_font) return;
    
    renderButtons(window, entity, player);
    
    Building* building = dynamic_cast<Building*>(entity.get());
    if (building && building->isProducing()) {
        renderProductionQueue(window, building);
    }
}

void ActionBar::renderButtons(sf::RenderWindow& window, EntityPtr entity, Player& player) {
    const auto& registryActions = ENTITY_DATA.getActions(entity->getType());
    
    Building* building = dynamic_cast<Building*>(entity.get());
    Unit* unit = dynamic_cast<Unit*>(entity.get());
    
    float buttonX = getPanelX() + Constants::ACTION_BAR_PADDING;
    float buttonY = getButtonY();
    float buttonSize = Constants::ACTION_BAR_BUTTON_SIZE;
    float buttonSpacing = Constants::ACTION_BAR_BUTTON_SPACING;
    
    for (size_t i = 0; i < registryActions.size(); ++i) {
        const auto& action = registryActions[i];
        
        bool isActive = false;
        bool isAvailable = true;
        
        // Determine active state and availability
        if (unit) {
            UnitState state = unit->getState();
            switch (action.type) {
                case ActionDef::Type::TargetMove:
                    isActive = (state == UnitState::Moving);
                    break;
                case ActionDef::Type::Instant:
                    isActive = (state == UnitState::Idle);
                    break;
                case ActionDef::Type::TargetAttack:
                    isActive = (state == UnitState::Attacking);
                    break;
                case ActionDef::Type::TargetGather:
                    isActive = (state == UnitState::Gathering || state == UnitState::Returning);
                    break;
                default:
                    break;
            }
        } else if (building) {
            if (action.type == ActionDef::Type::Train) {
                int mineralCost = ENTITY_DATA.getMineralCost(action.producesType);
                int gasCost = ENTITY_DATA.getGasCost(action.producesType);
                isAvailable = player.canAfford(mineralCost, gasCost);
                if (i == 0 && building->isProducing()) {
                    isActive = true;
                }
            }
        }
        
        // Button background
        sf::RectangleShape button(sf::Vector2f(buttonSize, buttonSize));
        button.setPosition(sf::Vector2f(buttonX, buttonY));
        
        sf::Color buttonColor;
        if (!isAvailable) {
            buttonColor = sf::Color(40, 40, 40);
        } else if (isActive) {
            buttonColor = sf::Color(60, 100, 60);
        } else {
            buttonColor = sf::Color(50, 50, 60);
        }
        button.setFillColor(buttonColor);
        button.setOutlineThickness(1.0f);
        button.setOutlineColor(isActive ? sf::Color(100, 180, 100) : sf::Color(70, 70, 80));
        window.draw(button);
        
        // Action label
        std::string shortLabel = action.label.substr(0, 3);
        sf::Text labelText(*m_font, shortLabel, 11);
        labelText.setFillColor(isAvailable ? sf::Color::White : sf::Color(100, 100, 100));
        sf::FloatRect textBounds = labelText.getLocalBounds();
        labelText.setPosition(sf::Vector2f(
            buttonX + (buttonSize - textBounds.size.x) / 2.0f,
            buttonY + 8.0f
        ));
        window.draw(labelText);
        
        // Hotkey
        sf::Text hotkeyText(*m_font, "[" + action.hotkey + "]", 9);
        hotkeyText.setFillColor(sf::Color(150, 150, 150));
        sf::FloatRect hkBounds = hotkeyText.getLocalBounds();
        hotkeyText.setPosition(sf::Vector2f(
            buttonX + (buttonSize - hkBounds.size.x) / 2.0f,
            buttonY + buttonSize - 18.0f
        ));
        window.draw(hotkeyText);
        
        buttonX += buttonSize + buttonSpacing;
    }
}

void ActionBar::renderProductionQueue(sf::RenderWindow& window, Building* building) {
    float panelX = getPanelX();
    float queueY = getQueueY();
    float queueX = panelX + Constants::ACTION_BAR_PADDING;
    
    std::vector<EntityType> queue = building->getProductionQueue();
    if (queue.empty()) return;
    
    // Current unit being trained (larger icon)
    sf::RectangleShape mainIcon(sf::Vector2f(MAIN_ICON_SIZE, MAIN_ICON_SIZE));
    mainIcon.setPosition(sf::Vector2f(queueX, queueY));
    mainIcon.setFillColor(sf::Color(60, 100, 60));
    mainIcon.setOutlineThickness(2.0f);
    mainIcon.setOutlineColor(sf::Color(100, 180, 100));
    window.draw(mainIcon);
    
    // Unit type label
    std::string unitLabel = ENTITY_DATA.getShortName(queue[0]);
    sf::Text mainLabelText(*m_font, unitLabel, 14);
    mainLabelText.setFillColor(sf::Color::White);
    sf::FloatRect mainLabelBounds = mainLabelText.getLocalBounds();
    mainLabelText.setPosition(sf::Vector2f(
        queueX + (MAIN_ICON_SIZE - mainLabelBounds.size.x) / 2.0f,
        queueY + (MAIN_ICON_SIZE - mainLabelBounds.size.y) / 2.0f - 4.0f
    ));
    window.draw(mainLabelText);
    
    // Progress bar
    float barWidth = MAIN_ICON_SIZE;
    float barHeight = 4.0f;
    float progress = building->getProductionProgress();
    
    sf::RectangleShape barBg(sf::Vector2f(barWidth, barHeight));
    barBg.setPosition(sf::Vector2f(queueX, queueY + MAIN_ICON_SIZE + 2.0f));
    barBg.setFillColor(sf::Color(40, 40, 40));
    window.draw(barBg);
    
    sf::RectangleShape barFill(sf::Vector2f(barWidth * progress, barHeight));
    barFill.setPosition(sf::Vector2f(queueX, queueY + MAIN_ICON_SIZE + 2.0f));
    barFill.setFillColor(sf::Color(80, 180, 80));
    window.draw(barFill);
    
    // Queued units (smaller icons)
    float queuedX = queueX + MAIN_ICON_SIZE + ICON_SPACING * 2;
    for (size_t i = 1; i < queue.size() && i < 6; ++i) {
        sf::RectangleShape queuedIcon(sf::Vector2f(QUEUED_ICON_SIZE, QUEUED_ICON_SIZE));
        queuedIcon.setPosition(sf::Vector2f(queuedX, queueY + (MAIN_ICON_SIZE - QUEUED_ICON_SIZE) / 2.0f));
        queuedIcon.setFillColor(sf::Color(50, 50, 60));
        queuedIcon.setOutlineThickness(1.0f);
        queuedIcon.setOutlineColor(sf::Color(70, 70, 80));
        window.draw(queuedIcon);
        
        std::string queuedLabel = ENTITY_DATA.getShortName(queue[i]);
        sf::Text queuedLabelText(*m_font, queuedLabel, 10);
        queuedLabelText.setFillColor(sf::Color(180, 180, 180));
        sf::FloatRect queuedLabelBounds = queuedLabelText.getLocalBounds();
        queuedLabelText.setPosition(sf::Vector2f(
            queuedX + (QUEUED_ICON_SIZE - queuedLabelBounds.size.x) / 2.0f,
            queueY + (MAIN_ICON_SIZE - QUEUED_ICON_SIZE) / 2.0f + (QUEUED_ICON_SIZE - queuedLabelBounds.size.y) / 2.0f - 2.0f
        ));
        window.draw(queuedLabelText);
        
        queuedX += QUEUED_ICON_SIZE + ICON_SPACING;
    }
    
    // Show count if more than 5 queued
    if (queue.size() > 6) {
        sf::Text moreText(*m_font, "+" + std::to_string(queue.size() - 6), 9);
        moreText.setFillColor(sf::Color(150, 150, 150));
        moreText.setPosition(sf::Vector2f(queuedX, queueY + (MAIN_ICON_SIZE - 10) / 2.0f));
        window.draw(moreText);
    }
    
    // Cancel hint
    sf::Text cancelText(*m_font, "[ESC] Cancel", 9);
    cancelText.setFillColor(sf::Color(180, 100, 100));
    cancelText.setPosition(sf::Vector2f(panelX + Constants::ACTION_BAR_WIDTH - 70.0f, queueY + 30.0f));
    window.draw(cancelText);
}
