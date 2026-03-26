#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>
#include <imgui.h>

int main()
{
    sf::RenderWindow window(
        sf::VideoMode({ 800u, 600u }),
        "CircuitLab"
    );
    window.setFramerateLimit(60);

    if (!ImGui::SFML::Init(window))
        return -1;

    sf::Clock deltaClock;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(window, *event);

            if (event->is<sf::Event::Closed>())
                window.close();
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        // Finestra di test ImGui
        ImGui::Begin("CircuitLab - Test");
        ImGui::Text("SFML + ImGui funzionano!");
        ImGui::End();

        window.clear(sf::Color(30, 30, 30));
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}