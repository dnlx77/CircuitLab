#include <imgui-SFML.h>
#include <imgui.h>
#include <implot.h>
#include <numbers>

#include "UI/Ui.h"
#include "Core/Vector2.h"
#include "Common/Logger.h"

// Determina quale componente o terminale si trova sotto il punto cliccato.
// Controlla prima i terminali (area più piccola, priorità alta),
// poi il corpo del componente (rettangolo centrale).
// Aggiorna selComp con il risultato; se nulla è trovato, imposta state = none.
void CircuitLab::UI::CheckClick(sf::Vector2i pos, SelecetedComponent &selComp)
{
	for (const auto &comp : m_componentViewList)
	{
		ComponentDesign des = comp.GetComponetDesign();

		float cosAngle = static_cast<float>(std::cos(-1 * comp.GetRotation() * std::numbers::pi / 180.0));
		float sinAngle = static_cast<float>(std::sin(-1 * comp.GetRotation() * std::numbers::pi / 180.0));

		int dx = static_cast<int>(pos.x - comp.GetPosition().x);
		int dy = static_cast<int>(pos.y - comp.GetPosition().y);

		float x1 = dx * cosAngle - dy * sinAngle;
		float y1 = dx * sinAngle + dy * cosAngle;

		int i = 0;
		for (const auto &terminal : des.terminalOffset)
		{
			// Click dentro la zona di tolleranza del terminale?
			if ((x1 >= terminal.x - CLICK_TOLLERANCE) && (x1 <= terminal.x + CLICK_TOLLERANCE) &&
				(y1 >= terminal.y - CLICK_TOLLERANCE) && (y1 <= terminal.y + CLICK_TOLLERANCE))
			{
				selComp.compId = comp.GetComponentLink();
				selComp.terminalIndex = i;
				selComp.linkId = -1;
				selComp.nodeViewId = -1;
				selComp.state = SelectionState::terminalSelected;
				selComp.clickPos = sf::Vector2f({ static_cast<float>(pos.x), static_cast<float>(pos.y) });
				return;
			}
			i++;
		}

		// Click dentro il rettangolo del corpo del componente?
		if ((x1 >= -des.compWidth / 2) &&
			(x1 <= des.compWidth / 2) &&
			(y1 >= -des.compHeight / 2) &&
			(y1 <= des.compHeight / 2))
		{
			selComp.compId = comp.GetComponentLink();
			selComp.terminalIndex = -1;
			selComp.linkId = -1;
			selComp.nodeViewId = -1;
			selComp.state = SelectionState::componentSelected;
			selComp.clickPos = sf::Vector2f({ static_cast<float>(pos.x), static_cast<float>(pos.y) });
			return;
		}
	}

	// click su un linkView
	sf::Vector2f posF(static_cast<float>(pos.x), static_cast<float>(pos.y));
	for (auto const &link : m_linkViewList)
	{
		NodeView nv = GetNodeViewFromLInkId(link.id);
		sf::Vector2f diffVec({ link.targetPos.x - link.startPos.x, link.targetPos.y - link.startPos.y });

		if (diffVec == sf::Vector2f(0.f, 0.f))
			continue;
		sf::Vector2f unitaryVec = diffVec.normalized();
		sf::Vector2f shrunkA({ link.startPos.x + unitaryVec.x * CLICK_TOLLERANCE, link.startPos.y + unitaryVec.y * CLICK_TOLLERANCE });
		sf::Vector2f shrunkB({ link.targetPos.x - unitaryVec.x * CLICK_TOLLERANCE, link.targetPos.y - unitaryVec.y * CLICK_TOLLERANCE });

		float dist = PointToStraightDistance(shrunkA, shrunkB, posF);

		if (dist <= CLICK_TOLLERANCE &&
			posF.x >= std::min(shrunkA.x, shrunkB.x) - EPSILON &&
			posF.x <= std::max(shrunkA.x, shrunkB.x) + EPSILON &&
			posF.y >= std::min(shrunkA.y, shrunkB.y) - EPSILON &&
			posF.y <= std::max(shrunkA.y, shrunkB.y) + EPSILON)
		{
			selComp.compId = -1;
			selComp.terminalIndex = -1;
			selComp.linkId = link.id;
			selComp.nodeViewId = -1;
			selComp.state = SelectionState::linkSelected;
			selComp.clickPos = sf::Vector2f({ static_cast<float>(pos.x), static_cast<float>(pos.y) });
			LOG_DEBUG("CheckClick found link id: " << link.id);
			return;
		}

	}

	// click su un nodeView
	for (const auto &nv : m_nodeViewList)
	{
		if (nv.linkViewIds.size() > 2)
		{
			// Click dentro la zona di tolleranza del terminale?
			if ((posF.x >= nv.position.x - CLICK_TOLLERANCE) && (posF.x <= nv.position.x + CLICK_TOLLERANCE) &&
				(posF.y >= nv.position.y - CLICK_TOLLERANCE) && (posF.y <= nv.position.y + CLICK_TOLLERANCE))
			{
				selComp.compId = -1;
				selComp.terminalIndex = -1;
				selComp.linkId = -1;
				selComp.nodeViewId = nv.id;
				selComp.state = SelectionState::nodeViewSelected;
				selComp.clickPos = sf::Vector2f({ posF.x, posF.y });
				return;
			}
		}
	}

	// Nessun componente trovato sotto il click
	selComp.compId = -1;
	selComp.terminalIndex = -1;
	selComp.linkId = -1;
	selComp.nodeViewId = -1;
	selComp.state = SelectionState::none;
	selComp.clickPos = sf::Vector2f({ posF.x, posF.y });
}

// Calcola le coordinate pixel dei due estremi di un collegamento tra terminali e nodeView.
// L'offset verticale del raggio sposta il punto di attacco sul bordo del cerchio
// del terminale, non sul suo centro.
CircuitLab::LinkView CircuitLab::UI::GetLinkCoords(int comp1, int term1, NodeView nodeView)
{
	sf::Vector2f pA, pB;

	for (const auto &comp : m_componentViewList)
	{
		if (comp.GetComponentLink() == comp1)
		{
			sf::Vector2f rotTerm = GetRotatedTerminalPos(comp, term1);
			pA.x = comp.GetPosition().x + rotTerm.x;
			pA.y = comp.GetPosition().y + rotTerm.y;
		}

	}

	LinkView link;
	link.id = ++m_linkViewIdCount;
	link.startPos = pA;
	link.targetPos = nodeView.position;
	link.compIdA = comp1;
	link.termIndexA = term1;
	link.nodeViewId = nodeView.id;
	return link;
}

// Calcola la posizione ruotata di un terminale rispetto al centro del componente.
// Applica la matrice di rotazione 2D all'offset del terminale,
// tenendo conto del raggio per attaccare il filo al bordo del cerchio.
sf::Vector2f CircuitLab::UI::GetRotatedTerminalPos(const ComponentView &cw, int termIndex) const
{
	ComponentDesign des = cw.GetComponetDesign();
	float cosAngle = static_cast<float>(std::cos(cw.GetRotation() * std::numbers::pi / 180.0));
	float sinAngle = static_cast<float>(std::sin(cw.GetRotation() * std::numbers::pi / 180.0));

	float x = static_cast<float>(des.terminalOffset[termIndex].x);
	float y = static_cast<float>(des.terminalOffset[termIndex].y + (des.terminalOffset[termIndex].y >= 0 ? 1 : -1) * des.terminalRadius);
	float x1 = x * cosAngle - y * sinAngle;
	float y1 = x * sinAngle + y * cosAngle;

	return sf::Vector2f({ x1, y1 });
}

void CircuitLab::UI::UpdateLinksForComponent(int compId)
{
	for (auto &cv : m_componentViewList)
	{
		if (cv.GetComponentLink() == compId)
		{
			ComponentDesign des = cv.GetComponetDesign();
			for (int i = 0; i < des.terminalOffset.size(); i++)
			{
				sf::Vector2f rotTer = GetRotatedTerminalPos(cv, i);
				sf::Vector2f posTer;
				posTer.x = cv.GetPosition().x + rotTer.x;
				posTer.y = cv.GetPosition().y + rotTer.y;

				for (auto &lv : m_linkViewList)
					if (lv.compIdA == compId && lv.termIndexA == i)
					{
						lv.startPos = posTer;

						// Se il nodeView è fantasma, segue il terminale
						for (auto &nv : m_nodeViewList)
							if (nv.id == lv.nodeViewId && nv.linkViewIds.size() <= 2)
							{
								nv.position = posTer;
								// Aggiorna targetPos di tutti i link collegati al nodeView
								for (int lvId : nv.linkViewIds)
									for (auto &otherLv : m_linkViewList)
										if (otherLv.id == lvId)
											otherLv.targetPos = posTer;
								break;
							}
					}
			}
		}
	}
}

std::vector<sf::Vector2f> CircuitLab::UI::GetTerminalPositionbyCompId(int compId) const
{
	std::vector<sf::Vector2f> terminalsPos;
	for (auto &cv : m_componentViewList)
	{
		if (cv.GetComponentLink() == compId)
		{
			ComponentDesign des = cv.GetComponetDesign();
			for (int i = 0; i < des.terminalOffset.size(); i++)
			{
				sf::Vector2f rotTer = GetRotatedTerminalPos(cv, i);
				sf::Vector2f terPos;
				terPos.x = cv.GetPosition().x + rotTer.x;
				terPos.y = cv.GetPosition().y + rotTer.y;
				terminalsPos.emplace_back(terPos);
			}
			return terminalsPos;
		}
	}
	return terminalsPos;
}

int CircuitLab::UI::GetNodeViewIdByLinkId(int linkId) const
{
	for (const auto lv : m_linkViewList)
		if (lv.id == linkId)
			return lv.nodeViewId;
	return -1;
}

CircuitLab::NodeView CircuitLab::UI::GetNodeViewById(int nodeViewId) const
{
	for (const auto &nv : m_nodeViewList)
		if (nv.id == nodeViewId)
			return nv;

	throw std::runtime_error("NodeView not found for id: " + std::to_string(nodeViewId));
}

void CircuitLab::UI::UpdateNodeViewLinkIds(int nodeViewId, std::vector<int> linkViewIds)
{
	for (auto &nv : m_nodeViewList)
		if (nv.id == nodeViewId)
		{
			nv.linkViewIds = linkViewIds;
			return;
		}
}

void CircuitLab::UI::HandleEvents()
{
	// --- Gestione eventi ---
	while (const std::optional event = m_window.pollEvent())
	{
		ImGui::SFML::ProcessEvent(m_window, *event);

		if (event->is<sf::Event::Closed>())
			m_window.close();

		else if (const auto *mouseEvent = event->getIf<sf::Event::MouseButtonPressed>())
		{
			auto pos = mouseEvent->position;

			if (mouseEvent->button == sf::Mouse::Button::Left && (m_selectedComponent.state != SelectionState::draggingComponent || m_selectedComponent.state != SelectionState::draggingNodeView))
			{
				// Aggiunta componenti con tasto modificatore + click
				if (pos.x < static_cast<int>(m_width - PANEL_WIDTH))
				{
					if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::R))
					{
						int id = m_onCircuitChange(ComponentType::resistor);
						AddViewComponent(id, "Resistor", ComponentType::resistor, Vec2(static_cast<float>(pos.x), static_cast<float>(pos.y)), DEFAULT_ROTATION);
					}
					if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V))
					{
						int id = m_onCircuitChange(ComponentType::voltageGenerator);
						AddViewComponent(id, "Voltage source", ComponentType::voltageGenerator, Vec2(static_cast<float>(pos.x), static_cast<float>(pos.y)), DEFAULT_ROTATION);
					}
					if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::G))
					{
						int id = m_onCircuitChange(ComponentType::ground);
						AddViewComponent(id, "Ground", ComponentType::ground, Vec2(static_cast<float>(pos.x), static_cast<float>(pos.y)), DEFAULT_ROTATION);
					}
				}

				// Gestione selezione e collegamento terminali:
				// - 1° click su un terminale: lo seleziona
				// - 2° click su un altro terminale: crea il collegamento
				if (m_selectedComponent.state != SelectionState::terminalSelected && m_selectedComponent.state != SelectionState::linkSelected && !ImGui::GetIO().WantCaptureMouse)
				{
					CheckClick(pos, m_selectedComponent);
				}
				else if (m_selectedComponent.state == SelectionState::terminalSelected)
				{
					// Salva i dati del primo terminale selezionato
					int comp1 = m_selectedComponent.compId;
					int term1 = m_selectedComponent.terminalIndex;

					SelecetedComponent temp;
					if (!ImGui::GetIO().WantCaptureMouse) CheckClick(pos, temp);

					if (temp.state == SelectionState::terminalSelected)
					{
						int comp2 = temp.compId;
						int term2 = temp.terminalIndex;
						int nodeIdTerm1 = m_onGetCompTerminalId(comp1)[term1];
						int nodeIdTerm2 = m_onGetCompTerminalId(comp2)[term2];

						if (comp1 != comp2 && (nodeIdTerm1 != nodeIdTerm2 || (nodeIdTerm1 == -1 && nodeIdTerm2 == -1)))
						{
							bool isConnect = m_onCreateLink(comp1, term1, comp2, term2);

							if (isConnect)
							{
								int mathNodeId = m_onGetCompTerminalId(comp1)[term1];

								int nvId1 = GetNodeViewIdByTerminal(comp1, term1);
								int nvId2 = GetNodeViewIdByTerminal(comp2, term2);

								sf::Vector2f comp1TermPos = GetTerminalPositionbyCompId(comp1)[term1];
								sf::Vector2f comp2TermPos = GetTerminalPositionbyCompId(comp2)[term2];

								if (nvId1 == -1 && nvId2 == -1)
								{
									// Scenario 1: nessun nodeView esistente
									//sf::Vector2f newPos = { (comp1TermPos.x + comp2TermPos.x) / 2.0f,
									//						(comp1TermPos.y + comp2TermPos.y) / 2.0f };

									sf::Vector2f newPos = comp2TermPos;

									NodeView newNV;
									newNV.id = ++m_nodeViewCount;
									newNV.nodeId = mathNodeId;
									newNV.position = newPos;

									LinkView lv1, lv2;
									lv1.id = ++m_linkViewIdCount;
									lv1.startPos = comp1TermPos;
									lv1.targetPos = comp2TermPos;
									lv1.compIdA = comp1;
									lv1.termIndexA = term1;
									lv1.nodeViewId = newNV.id;

									lv2.id = ++m_linkViewIdCount;
									lv2.startPos = comp2TermPos;
									lv2.targetPos = comp2TermPos;
									lv2.compIdA = comp2;
									lv2.termIndexA = term2;
									lv2.nodeViewId = newNV.id;

									newNV.linkViewIds.push_back(lv1.id);
									newNV.linkViewIds.push_back(lv2.id);

									m_nodeViewList.push_back(newNV);
									m_linkViewList.push_back(lv1);
									m_linkViewList.push_back(lv2);
								}
								else if (nvId1 != -1 && nvId2 == -1)
								{
									// Scenario 2: term1 ha già un nodeView
									LinkView lv2;
									lv2.id = ++m_linkViewIdCount;
									lv2.startPos = comp2TermPos;
									lv2.targetPos = GetNodeViewById(nvId1).position;
									lv2.compIdA = comp2;
									lv2.termIndexA = term2;
									lv2.nodeViewId = nvId1;

									m_linkViewList.push_back(lv2);

									for (auto &nv : m_nodeViewList)
										if (nv.id == nvId1)
										{
											nv.linkViewIds.push_back(lv2.id);
											break;
										}
								}
								else if (nvId1 == -1 && nvId2 != -1)
								{
									// Scenario 3: term2 ha già un nodeView
									LinkView lv1;
									lv1.id = ++m_linkViewIdCount;
									lv1.startPos = comp1TermPos;
									lv1.targetPos = GetNodeViewById(nvId2).position;
									lv1.compIdA = comp1;
									lv1.termIndexA = term1;
									lv1.nodeViewId = nvId2;

									m_linkViewList.push_back(lv1);

									for (auto &nv : m_nodeViewList)
										if (nv.id == nvId2)
										{
											nv.linkViewIds.push_back(lv1.id);
											break;
										}
								}
								else if (nvId1 != -1 && nvId2 != -1 && nvId1 != nvId2)
								{
									// Scenario 4: merge dei due nodeView
									auto itNv2 = std::find_if(m_nodeViewList.begin(), m_nodeViewList.end(),
										[nvId2](const NodeView &nv) { return nv.id == nvId2; });

									if (itNv2 != m_nodeViewList.end())
									{
										for (int linkIdToMove : itNv2->linkViewIds)
										{
											for (auto &lv : m_linkViewList)
												if (lv.id == linkIdToMove)
												{
													lv.nodeViewId = nvId1;
													lv.targetPos = GetNodeViewById(nvId1).position;
													break;
												}

											for (auto &nv : m_nodeViewList)
												if (nv.id == nvId1)
												{
													nv.linkViewIds.push_back(linkIdToMove);
													break;
												}
										}
										m_nodeViewList.erase(itNv2);
									}
								}
							}
						}

						m_selectedComponent.state = SelectionState::none;
						m_selectedComponent.compId = -1;
						m_selectedComponent.terminalIndex = -1;
						m_selectedComponent.nodeViewId = -1;
					}
					else if (temp.state == SelectionState::linkSelected || temp.state == SelectionState::nodeViewSelected)
					{
						bool isDuplicated = false;
						// secondo click su link dopo primo su terminale
						for (const auto &lv : m_linkViewList)
						{
							if (lv.compIdA == m_selectedComponent.compId && lv.termIndexA == m_selectedComponent.terminalIndex)
							{
								isDuplicated = true;
								break;
							}
						}	
						if (!isDuplicated)
						{
							LinkView newLink;
							
							newLink.id = ++m_linkViewIdCount;
							newLink.startPos = GetTerminalPositionbyCompId(m_selectedComponent.compId)[m_selectedComponent.terminalIndex];
							newLink.compIdA = m_selectedComponent.compId;
							newLink.termIndexA = m_selectedComponent.terminalIndex;
							if (temp.state == SelectionState::linkSelected)
								newLink.nodeViewId = GetNodeViewIdByLinkId(temp.linkId);
							else
								newLink.nodeViewId = temp.nodeViewId;

							newLink.targetPos = GetNodeViewById(newLink.nodeViewId).position;

							m_linkViewList.emplace_back(newLink);

							for (auto &nv:m_nodeViewList)
								if (nv.id == newLink.nodeViewId)
								{
									nv.linkViewIds.emplace_back(newLink.id);
									break;
								}

							// Trova un compId già collegato al nodeView per notificare Circuit
							NodeView nv;
							if (temp.state == SelectionState::linkSelected)
								nv = GetNodeViewFromLInkId(temp.linkId);
							else
								nv = GetNodeViewById(temp.nodeViewId);
							int existingCompId = -1;
							int existingTermIndex = -1;
							for (const auto &lv : m_linkViewList)
								if (lv.nodeViewId == nv.id)
								{
									existingCompId = lv.compIdA;
									existingTermIndex = lv.termIndexA;
									break;
								}

							m_onCreateLink(m_selectedComponent.compId, m_selectedComponent.terminalIndex, existingCompId, existingTermIndex);
						}
						else
						{
							int nvId1 = GetNodeViewIdByTerminal(m_selectedComponent.compId, m_selectedComponent.terminalIndex);
							int nvId2 = (temp.state == SelectionState::linkSelected) ?
								GetNodeViewIdByLinkId(temp.linkId) :
								temp.nodeViewId;

							// Scenario 4: merge
							auto itNv2 = std::find_if(m_nodeViewList.begin(), m_nodeViewList.end(),
								[nvId2](const NodeView &nv) { return nv.id == nvId2; });

							if (itNv2 != m_nodeViewList.end() && nvId1 != nvId2)
							{
								for (int linkIdToMove : itNv2->linkViewIds)
								{
									for (auto &lv : m_linkViewList)
										if (lv.id == linkIdToMove)
										{
											lv.nodeViewId = nvId1;
											lv.targetPos = GetNodeViewById(nvId1).position;
											break;
										}

									for (auto &nv : m_nodeViewList)
										if (nv.id == nvId1)
										{
											nv.linkViewIds.push_back(linkIdToMove);
											break;
										}
								}
								m_nodeViewList.erase(itNv2);

								// Notifica Circuit
								int existingCompId = -1, existingTermIndex = -1;
								for (const auto &lv : m_linkViewList)
									if (lv.nodeViewId == nvId1 && !(lv.compIdA == m_selectedComponent.compId && lv.termIndexA == m_selectedComponent.terminalIndex))
									{
										existingCompId = lv.compIdA;
										existingTermIndex = lv.termIndexA;
										break;
									}
								m_onCreateLink(m_selectedComponent.compId, m_selectedComponent.terminalIndex, existingCompId, existingTermIndex);
							}
						}
							
						// Reset selezione
						m_selectedComponent.state = SelectionState::none;
						m_selectedComponent.compId = -1;
						m_selectedComponent.terminalIndex = -1;
						m_selectedComponent.nodeViewId = -1;
					}
				}
				else if (m_selectedComponent.state == SelectionState::linkSelected || m_selectedComponent.state == SelectionState::nodeViewSelected)
				{
					// Ho cliccato su un terminale dopo aver cliccato su un link devo creare il nodeview
					SelecetedComponent temp;
					if (!ImGui::GetIO().WantCaptureMouse) CheckClick(pos, temp);
		
					if (temp.state == SelectionState::terminalSelected)
					{
						bool isDuplicated = false;
						// secondo click su terminale dopo primo su link
						for (const auto &lv : m_linkViewList)
						{
							if (lv.compIdA == temp.compId && lv.termIndexA == temp.terminalIndex)
							{
								isDuplicated = true;
								break;
							}
						}
						
						if (!isDuplicated)
						{
							LinkView newLink;
							newLink.id = ++m_linkViewIdCount;
							newLink.startPos = GetTerminalPositionbyCompId(temp.compId)[temp.terminalIndex];
							newLink.compIdA = temp.compId;
							newLink.termIndexA = temp.terminalIndex;
							if (m_selectedComponent.state == SelectionState::linkSelected)
								newLink.nodeViewId = GetNodeViewIdByLinkId(m_selectedComponent.linkId);
							else
								newLink.nodeViewId = m_selectedComponent.nodeViewId;

							newLink.targetPos = GetNodeViewById(newLink.nodeViewId).position;

							m_linkViewList.emplace_back(newLink);

							for (auto &nv : m_nodeViewList)
								if (nv.id == newLink.nodeViewId)
								{
									nv.linkViewIds.emplace_back(newLink.id);
									break;
								}

							// Trova un compId già collegato al nodeView per notificare Circuit
							NodeView nv;
							if (m_selectedComponent.state == SelectionState::linkSelected)
								nv = GetNodeViewFromLInkId(m_selectedComponent.linkId);
							else
								nv = GetNodeViewById(m_selectedComponent.nodeViewId);
							int existingCompId = -1;
							int existingTermIndex = -1;
							for (const auto &lv : m_linkViewList)
								if (lv.nodeViewId == nv.id && lv.id != newLink.id)
								{
									existingCompId = lv.compIdA;
									existingTermIndex = lv.termIndexA;
									break;
								}

							m_onCreateLink(temp.compId, temp.terminalIndex, existingCompId, existingTermIndex);
						}
						else
						{
							int nvId1 = (m_selectedComponent.state == SelectionState::linkSelected) ?
								GetNodeViewIdByLinkId(m_selectedComponent.linkId) :
								m_selectedComponent.nodeViewId;
							int nvId2 = GetNodeViewIdByTerminal(temp.compId, temp.terminalIndex);

							// Scenario 4: merge
							auto itNv2 = std::find_if(m_nodeViewList.begin(), m_nodeViewList.end(),
								[nvId2](const NodeView &nv) { return nv.id == nvId2; });

							if (itNv2 != m_nodeViewList.end() && nvId1 != nvId2)
							{
								for (int linkIdToMove : itNv2->linkViewIds)
								{
									for (auto &lv : m_linkViewList)
										if (lv.id == linkIdToMove)
										{
											lv.nodeViewId = nvId1;
											lv.targetPos = GetNodeViewById(nvId1).position;
											break;
										}

									for (auto &nv : m_nodeViewList)
										if (nv.id == nvId1)
										{
											nv.linkViewIds.push_back(linkIdToMove);
											break;
										}
								}
								m_nodeViewList.erase(itNv2);

								// Notifica Circuit
								int existingCompId = -1, existingTermIndex = -1;
								for (const auto &lv : m_linkViewList)
									if (lv.nodeViewId == nvId1 && !(lv.compIdA == m_selectedComponent.compId && lv.termIndexA == m_selectedComponent.terminalIndex))
									{
										existingCompId = lv.compIdA;
										existingTermIndex = lv.termIndexA;
										break;
									}
								m_onCreateLink(temp.compId, temp.terminalIndex, existingCompId, existingTermIndex);
							}
						}

						// Reset selezione
						m_selectedComponent.state = SelectionState::none;
						m_selectedComponent.compId = -1;
						m_selectedComponent.nodeViewId = -1;
						m_selectedComponent.terminalIndex = -1;
					}
				}
			}
			else if (mouseEvent->button == sf::Mouse::Button::Right)
			{
				if (!ImGui::GetIO().WantCaptureMouse) CheckClick(pos, m_selectedComponent);
				if (m_selectedComponent.state == SelectionState::componentSelected || 
					m_selectedComponent.state == SelectionState::terminalSelected)
				{
					for (auto const &comp : m_componentViewList)
						if (comp.GetComponentLink() == m_selectedComponent.compId)
						{
							m_compClickOffset.x = pos.x - comp.GetPosition().x;
							m_compClickOffset.y = pos.y - comp.GetPosition().y;
						}
					m_selectedComponent.state = SelectionState::draggingComponent;
				}
				else if (m_selectedComponent.state == SelectionState::nodeViewSelected)
				{
					for (auto const &nv : m_nodeViewList)
						if (nv.id == m_selectedComponent.compId) 
						{
							m_compClickOffset.x = pos.x - nv.position.x;
							m_compClickOffset.y = pos.y - nv.position.y;
						}
					m_selectedComponent.state = SelectionState::draggingNodeView;
				}
			}
		}
		else if (const auto *mouseMovedEvent = event->getIf<sf::Event::MouseMoved>())
		{
			auto pos = mouseMovedEvent->position;
			Vec2 newPos;
			if (m_selectedComponent.state == SelectionState::draggingComponent)
			{
				newPos.x = std::clamp(pos.x - m_compClickOffset.x, 0.0f, static_cast<float>(m_width - PANEL_WIDTH));
				newPos.y = std::clamp(pos.y - m_compClickOffset.y, 0.0f, static_cast<float>(m_heigth));

				for (auto &cw : m_componentViewList)
					if (cw.GetComponentLink() == m_selectedComponent.compId)
						cw.SetPosition(newPos);

				UpdateLinksForComponent(m_selectedComponent.compId);
			}
			else if (m_selectedComponent.state == SelectionState::draggingNodeView)
			{
				newPos.x = std::clamp(pos.x - m_compClickOffset.x, 0.0f, static_cast<float>(m_width - PANEL_WIDTH));
				newPos.y = std::clamp(pos.y - m_compClickOffset.y, 0.0f, static_cast<float>(m_heigth));

				for (auto &nv : m_nodeViewList)
					if (nv.id == m_selectedComponent.nodeViewId)
					{
						nv.position.x = newPos.x;
						nv.position.y = newPos.y;

						UpdateLinksForNodeView(m_selectedComponent.compId, sf::Vector2f(newPos.x, newPos.y));
						break;
					}
			}
		}
		else if (const auto *mouseReleasedEvent = event->getIf<sf::Event::MouseButtonReleased>()) 
		{
			if (mouseReleasedEvent->button == sf::Mouse::Button::Right)
				m_selectedComponent.state = SelectionState::none;
		}
		else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Delete) && (m_selectedComponent.state != SelectionState::draggingComponent || m_selectedComponent.state != SelectionState::draggingNodeView))
		{
			// Eliminazione del componente selezionato con tasto Delete:
			// rimuove la vista, i fili collegati e notifica il circuito
			if (m_selectedComponent.state == SelectionState::componentSelected)
			{
				int id = m_selectedComponent.compId;

				// Rimuove la vista del componente
				m_componentViewList.erase(
					std::remove_if(m_componentViewList.begin(), m_componentViewList.end(),
						[id](const ComponentView &cw) {
							return cw.GetComponentLink() == id;
						}),
					m_componentViewList.end()
				);

				LOG_DEBUG("nodeView count before: " << m_nodeViewList.size());
				for (const auto &lv : m_linkViewList)
					if (lv.compIdA == id)
					{
						int nvId = lv.nodeViewId;
						if (RemoveLinkFromNodeView(nvId, lv.id) == 0)
						{
							LOG_DEBUG("removing nodeView: " << nvId);
							m_nodeViewList.erase(
								std::remove_if(m_nodeViewList.begin(), m_nodeViewList.end(),
									[nvId](const NodeView &nv) {
										return nv.id == nvId;
									}),
								m_nodeViewList.end()
							);
						}
					}
				LOG_DEBUG("nodeView count after: " << m_nodeViewList.size());

				// Rimuove i fili collegati al componente eliminato
				m_linkViewList.erase(
					std::remove_if(m_linkViewList.begin(), m_linkViewList.end(),
						[id](const LinkView &lw) {
							return (lw.compIdA == id);
						}),
					m_linkViewList.end()
				);

				m_onDeleteComponent(m_selectedComponent.compId);

				// Reset selezione
				m_selectedComponent.state = SelectionState::none;
				m_selectedComponent.compId = -1;
				m_selectedComponent.terminalIndex = -1;
				m_selectedComponent.nodeViewId = -1;
			}
		}
		else if (const auto *keyboardEvent = event->getIf<sf::Event::KeyPressed>())
		{
			if (m_selectedComponent.state == SelectionState::componentSelected && keyboardEvent->code == sf::Keyboard::Key::Q)
			{
				for (auto &cw : m_componentViewList)
					if (cw.GetComponentLink() == m_selectedComponent.compId)
						cw.SetRotation(static_cast<float>(static_cast<int>(cw.GetRotation() + 45) % 360));

				UpdateLinksForComponent(m_selectedComponent.compId);
			}
		}
	}
}

void CircuitLab::UI::DrawImageGuiPanel()
{
	ImGui::SetNextWindowPos({ static_cast<float>(m_width - PANEL_WIDTH), 0.0f });
	ImGui::SetNextWindowSize({ PANEL_WIDTH, static_cast<float>(m_heigth) });
	// --- Pannello ImGui ---
	ImGui::Begin("CircuitLab - Test", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

	if (ImGui::Button("Start Simulation"))
		m_onSetSimulationStatus(SimulationStatus::running);
	if (ImGui::Button("Pause Simulation"))
		m_onSetSimulationStatus(SimulationStatus::paused);
	if (ImGui::Button("Stop Simulation"))
		m_onSetSimulationStatus(SimulationStatus::stopped);

	if (ImGui::Button("New"))
		m_onNew();

	static char pathBuffer[256] = "circuit.json";
	ImGui::InputText("File", pathBuffer, sizeof(pathBuffer));

	if (ImGui::Button("Save"))
		m_onSave(pathBuffer);

	if (ImGui::Button("Load"))
		m_onLoad(pathBuffer);

	ImGui::Separator();
	if (ImGui::Button(m_showOscilloscope ? "Hide Oscilloscope" : "Show Oscilloscope"))
		m_showOscilloscope = !m_showOscilloscope;
	ImGui::Separator();

	// Mostra il risultato della simulazione o un messaggio di errore
	if (m_simulationOutput.simRes == SimulationResult::solve_error)
		ImGui::Text("Circuito non risolvibile!");
	else if (m_simulationOutput.simRes == SimulationResult::empty_circuit)
		ImGui::Text("Il circuito non contiene componenti!");
	else if (m_simulationOutput.simRes == SimulationResult::no_circuit)
		ImGui::Text("Errore interno, puntatore a circuito nullo!");
	else if (m_simulationOutput.simRes == SimulationResult::only_ground_circuit)
		ImGui::Text("Il circuito contiene solo componenti ground!");
	else
	{
		ImGui::Text("Risultato: [");
		for (const auto &r : m_simulationOutput.res)
		{
			std::string res = r.first + " " + std::to_string(r.second) + " ";
			ImGui::Text(res.c_str());
		}
		ImGui::Text("]");

		ImGui::Text("Correnti: [");
		for (const auto &i : m_simulationOutput.currentComp)
		{
			std::string cur = "comp" + std::to_string(i.first) + " " + std::to_string(i.second) + " ";
			ImGui::Text(cur.c_str());
		}
		ImGui::Text("]");
	}

	if (m_selectedComponent.state == SelectionState::componentSelected)
	{
		float posX, posY, rot;
		bool edited = false;
		std::map<ComponentValue, double> values;
		for (auto &cw : m_componentViewList)
			if (cw.GetComponentLink() == m_selectedComponent.compId)
			{
				posX = cw.GetPosition().x;
				posY = cw.GetPosition().y;
				rot = cw.GetRotation();
				ImGui::InputFloat("Posizione X: ", &posX); if (ImGui::IsItemDeactivatedAfterEdit()) edited = true;
				ImGui::InputFloat("Posizione Y: ", &posY); if (ImGui::IsItemDeactivatedAfterEdit()) edited = true;
				ImGui::InputFloat("Rotazione: ", &rot); if (ImGui::IsItemDeactivatedAfterEdit()) edited = true;

				if (edited)
				{
					Vec2 newPos;
					newPos.x = posX;
					newPos.y = posY;
					cw.SetRotation(rot);
					cw.SetPosition(newPos);
					UpdateLinksForComponent(m_selectedComponent.compId);
				}
			}

		values = m_onGetComponentValues(m_selectedComponent.compId);
		for (auto &[key, value] : values)
		{
			std::string label(ComponentValueToString(key));
			ImGui::InputDouble(label.c_str(), &value);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				values.at(key) = value;
				m_onSetComponentValues(m_selectedComponent.compId, values);
			}
		}

		// Combo box waveform — visibile solo per VoltageGenerator
		WaveFormType currentWaveForm = m_onGetWaveFormType(m_selectedComponent.compId);
		if (currentWaveForm != WaveFormType::none)
		{
			const char *waveFormNames[] = { "DC", "Sine", "Square" };
			WaveFormType waveFormValues[] = {
				WaveFormType::dcWaveForm,
				WaveFormType::sineWaveForm,
				WaveFormType::squareWaveForm
			};

			constexpr int waveFormCount = static_cast<int>(std::size(waveFormNames));

			int currentIndex = 0;
			for (int i = 0; i < waveFormCount; i++)
				if (waveFormValues[i] == currentWaveForm)
					currentIndex = i;

			if (ImGui::Combo("Waveform", &currentIndex, waveFormNames, waveFormCount))
				m_onSetWaveFormType(m_selectedComponent.compId, waveFormValues[currentIndex]);
		}
		
	}

	ImGui::End();

	// Dopo ImGui::End() chiama DrawOscilloscope se visibile
	if (m_showOscilloscope)
		DrawOscilloscope();
}

void CircuitLab::UI::DrawComponents()
{
	// Disegna ogni componente: rettangolo colorato per tipo + cerchi per i terminali + etichetta
	for (const auto &comp : m_componentViewList)
	{
		ComponentDesign des = comp.GetComponetDesign();

		sf::RectangleShape rect;
		rect.setSize({ static_cast<float>(des.compWidth), static_cast<float>(des.compHeight) });
		rect.setOrigin({ static_cast<float>(des.compWidth / 2), static_cast<float>(des.compHeight / 2) });
		rect.setPosition({ comp.GetPosition().x, comp.GetPosition().y });
		rect.setRotation(sf::degrees(comp.GetRotation()));

		// Colore del corpo in base al tipo
		if (comp.GetComponentType() == ComponentType::resistor)
			rect.setFillColor(sf::Color::Green);
		else if (comp.GetComponentType() == ComponentType::voltageGenerator)
			rect.setFillColor(sf::Color::Red);
		else if (comp.GetComponentType() == ComponentType::ground)
			rect.setFillColor(sf::Color::White);

		// Outline giallo se il componente è selezionato (corpo, non terminale)
		if (comp.GetComponentLink() == m_selectedComponent.compId &&
			m_selectedComponent.terminalIndex == -1 && 
			m_selectedComponent.state == SelectionState::componentSelected)
		{
			rect.setOutlineColor(sf::Color::Yellow);
			rect.setOutlineThickness(OUTLINE_THICKNESS);
		}

		m_window.draw(rect);

		// Disegna i terminali come cerchi blu
		sf::CircleShape term(static_cast<float>(des.terminalRadius));
		term.setFillColor(sf::Color::Blue);
		term.setOrigin({ static_cast<float>(des.terminalRadius), static_cast<float>(des.terminalRadius) });
		std::vector<int> terminalsId = m_onGetCompTerminalId(comp.GetComponentLink());

		int strPosX = 0, strPosY = 0;

		for (int i = 0; i < des.terminalOffset.size(); i++)
		{
			sf::Text termLabel(m_font);

			sf::Vector2f rotTerm = GetRotatedTerminalPos(comp, i);

			strPosX = (rotTerm.x >= 0) ? 1 : -1;
			strPosY = (rotTerm.y >= 0) ? 1 : -1;

			term.setPosition({ comp.GetPosition().x + rotTerm.x,comp.GetPosition().y + rotTerm.y });

			// Outline giallo se questo terminale è selezionato
			if (comp.GetComponentLink() == m_selectedComponent.compId &&
				m_selectedComponent.terminalIndex == i &&
				m_selectedComponent.state==SelectionState::terminalSelected)
			{
				term.setOutlineColor(sf::Color::Yellow);
				term.setOutlineThickness(OUTLINE_THICKNESS);
			}

			m_window.draw(term);
			term.setOutlineThickness(0); // Reset per il prossimo terminale

			// Aggiunge il prefisso "+" se questo è il terminale positivo del componente
			std::string termString;
			if (des.isPositiveTerminal == i)
				termString = "+ ";
			termString += std::to_string(terminalsId[i]);
			termLabel.setString(termString);
			termLabel.setCharacterSize(12);
			termLabel.setPosition({ comp.GetPosition().x + rotTerm.x + strPosX * TEXT_COMPONENT_OFFSET, comp.GetPosition().y + rotTerm.y + strPosY * TEXT_COMPONENT_OFFSET });
			m_window.draw(termLabel);
		}

		// Costruisce l'etichetta del componente nel formato "R3" / "V2" / "G1"
		// usando il prefisso del tipo seguito dall'ID del componente
		std::string compString;
		if (comp.GetComponentType() == ComponentType::resistor)
			compString += "R";
		else if (comp.GetComponentType() == ComponentType::voltageGenerator)
			compString += "V";
		else if (comp.GetComponentType() == ComponentType::ground)
			compString += "G";

		compString += std::to_string(comp.GetComponentLink());

		float rot = comp.GetRotation();
		float cosAngle = static_cast<float>(std::cos(rot * std::numbers::pi / 180.0));
		float sinAngle = static_cast<float>(std::sin(rot * std::numbers::pi / 180.0));

		float x = 1.0f;
		float y = 0.0f;
		float x1 = x * cosAngle - y * sinAngle;
		float y1 = x * sinAngle + y * cosAngle;

		sf::Text label(m_font);
		label.setString(compString);
		label.setCharacterSize(12);

		sf::FloatRect bound = label.getLocalBounds();
		float originX, originY;
	
		if (x1 < 0)       originX = bound.size.x;
		else if (x1 == 0) originX = bound.size.x / 2;
		else              originX = 0;

		if (y1 < 0)       originY = bound.size.y;
		else if (y1 == 0) originY = bound.size.y / 2;
		else              originY = 0;
		
		label.setOrigin({ originX, originY });
		label.setPosition({ comp.GetPosition().x + x1 * TEXT_COMPONENT_OFFSET, comp.GetPosition().y + y1 * TEXT_COMPONENT_OFFSET });
		m_window.draw(label);
	}
}

void CircuitLab::UI::DrawWires()
{
	// Disegna i fili come linee bianche tra i punti dei terminali collegati
	for (const auto &wire : m_linkViewList)
	{
		sf::Vertex line[2] = {
			sf::Vertex{wire.startPos, (m_selectedComponent.state == SelectionState::linkSelected && wire.id == m_selectedComponent.linkId) ? sf::Color::Red : sf::Color::White},
			sf::Vertex{GetNodeviewPositionByNodeViewId(wire.nodeViewId), (m_selectedComponent.state == SelectionState::linkSelected && wire.id == m_selectedComponent.linkId) ? sf::Color::Red : sf::Color::White}
		};
		m_window.draw(line, 2, sf::PrimitiveType::Lines);
		
		DrawParticles(wire.id);
	}
}

void CircuitLab::UI::DrawNodes()
{
	for (const auto nv : m_nodeViewList)
	{
		// Disegna i nodi come cerchi verdi
		sf::CircleShape node(NODE_RADIUS);
		node.setFillColor(sf::Color::Green);
		node.setOrigin({ NODE_RADIUS, NODE_RADIUS });
		node.setPosition(nv.position);

		// Outline giallo se questo nodo è selezionato
		if (nv.id == m_selectedComponent.compId && m_selectedComponent.state==SelectionState::nodeViewSelected)
		{
			node.setOutlineColor(sf::Color::Yellow);
			node.setOutlineThickness(OUTLINE_THICKNESS);
		}

		m_window.draw(node);
		node.setOutlineThickness(0); // Reset per il prossimo nodo

		std::string nodeString;
	}
}

void CircuitLab::UI::DrawParticles(int linkId)
{
	for (const auto &lv : m_linkViewList)
	{
		if (lv.id == linkId)
		{
			sf::Vector2f nodeViewPos = GetNodeviewPositionByNodeViewId(lv.nodeViewId);
			for (const auto &lp : m_linkParticlesList)
			{
				if (lp.linkViewId == linkId)
				{
					for (int i = 0; i < lp.count; i++)
					{
						float t_i = std::fmod(lp.offset + static_cast<float>(i) / lp.count, 1.0f);
						sf::Vector2f pPos;
						pPos.x = lv.startPos.x + (nodeViewPos.x - lv.startPos.x) * t_i;
						pPos.y = lv.startPos.y + (nodeViewPos.y - lv.startPos.y) * t_i;
						sf::CircleShape particle(NODE_RADIUS);
						particle.setFillColor(sf::Color::Yellow);
						particle.setOrigin({ NODE_RADIUS, NODE_RADIUS });
						particle.setPosition({ pPos.x,pPos.y });
						m_window.draw(particle);
					}
				}
			}
		}
	}
}

void CircuitLab::UI::DrawOscilloscope()
{
	ImGui::Begin("Oscilloscope", &m_showOscilloscope);

	// Raccogli nodi disponibili
	std::vector<int> nodeIds;
	for (auto &[id, v] : m_simulationOutput.nodeVoltages)
		nodeIds.push_back(id);
	std::sort(nodeIds.begin(), nodeIds.end());

	// Raccogli componenti disponibili
	std::vector<int> compIds;
	for (auto &[id, v] : m_simulationOutput.currentComp)
		compIds.push_back(id);

	// Combo ProbeType
	const char *probeTypeNames[] = { "Node Voltage", "Differential Voltage", "Component Current", "Branch Current" };
	ImGui::Combo("Probe Type", &m_oscProbeType, probeTypeNames, std::size(probeTypeNames));

	ProbeType selectedType = static_cast<ProbeType>(m_oscProbeType);

	// Combo idA
	if (selectedType == ProbeType::nodeVoltage ||
		selectedType == ProbeType::differentialVoltage ||
		selectedType == ProbeType::branchCurrent)
	{
		std::vector<std::string> nodeLabels;
		for (int id : nodeIds) nodeLabels.push_back("Node " + std::to_string(id));
		std::vector<const char *> nodeLabelPtrs;
		for (auto &s : nodeLabels) nodeLabelPtrs.push_back(s.c_str());
		ImGui::Combo("Node A", &m_oscIdA, nodeLabelPtrs.data(), static_cast<int>(nodeLabelPtrs.size()));
	}
	else
	{
		std::vector<std::string> compLabels;
		for (int id : compIds) compLabels.push_back("Comp " + std::to_string(id));
		std::vector<const char *> compLabelPtrs;
		for (auto &s : compLabels) compLabelPtrs.push_back(s.c_str());
		ImGui::Combo("Component", &m_oscCompId, compLabelPtrs.data(), static_cast<int>(compLabelPtrs.size()));
	}

	// Combo idB
	if (selectedType == ProbeType::differentialVoltage ||
		selectedType == ProbeType::branchCurrent)
	{
		std::vector<std::string> nodeLabels;
		for (int id : nodeIds) nodeLabels.push_back("Node " + std::to_string(id));
		std::vector<const char *> nodeLabelPtrs;
		for (auto &s : nodeLabels) nodeLabelPtrs.push_back(s.c_str());
		ImGui::Combo("Node B", &m_oscIdB, nodeLabelPtrs.data(), static_cast<int>(nodeLabelPtrs.size()));
	}

	// Combo compId per branchCurrent
	if (selectedType == ProbeType::branchCurrent)
	{
		std::vector<std::string> compLabels;
		for (int id : compIds) compLabels.push_back("Comp " + std::to_string(id));
		std::vector<const char *> compLabelPtrs;
		for (auto &s : compLabels) compLabelPtrs.push_back(s.c_str());
		ImGui::Combo("Branch Component", &m_oscCompId, compLabelPtrs.data(), static_cast<int>(compLabelPtrs.size()));
	}

	// Pulsante aggiungi
	if (ImGui::Button("Add Channel"))
	{
		int idA = -1, idB = -1, compId = -1;

		if (selectedType == ProbeType::componentCurrent)
			compId = compIds.empty() ? -1 : compIds[m_oscCompId];
		else
			idA = nodeIds.empty() ? -1 : nodeIds[m_oscIdA];

		if (selectedType == ProbeType::differentialVoltage ||
			selectedType == ProbeType::branchCurrent)
			idB = nodeIds.empty() ? -1 : nodeIds[m_oscIdB];

		if (selectedType == ProbeType::branchCurrent)
			compId = compIds.empty() ? -1 : compIds[m_oscCompId];

		if (idA != -1 || compId != -1)
			m_onAddChannel(selectedType, idA, idB, compId);
	}

	ImGui::Separator();

	// Lista canali — FUORI dal blocco ImPlot
	auto channels = m_onGetOscilloscopeChannels();
	bool removedChannel = false;
	for (int i = 0; i < static_cast<int>(channels.size()); i++)
	{
		auto &channel = channels[i];

		bool active = channel.active;
		if (ImGui::Checkbox(("##active" + channel.label).c_str(), &active))
			m_onSetChannelActive(i, active);

		ImGui::SameLine();

		if (ImGui::Button(("X##" + channel.label).c_str()))
		{
			m_onRemoveChannel(i);
			removedChannel = true;
			break;
		}

		ImGui::SameLine();

		ImGui::TextColored(
			ImVec4(channel.channelColor.r, channel.channelColor.g, channel.channelColor.b, 1.0f),
			channel.label.c_str());
	}

	ImGui::Separator();

	// Plot — solo se non abbiamo appena rimosso un canale
	if (!removedChannel && ImPlot::BeginPlot("##oscilloscope", ImVec2(-1, 300)))
	{
		ImPlot::SetupAxes("Samples", "Value");

		for (auto &channel : channels)
		{
			if (!channel.active || channel.samples.empty())
				continue;

			std::vector<float> samples;
			samples.reserve(channel.samples.size());
			std::transform(channel.samples.begin(), channel.samples.end(),
				std::back_inserter(samples),
				[](double d) { return static_cast<float>(d); });

			ImPlotSpec spec;
			spec.LineColor = ImVec4(
				channel.channelColor.r,
				channel.channelColor.g,
				channel.channelColor.b,
				1.0f);

			ImPlot::PlotLine(channel.label.c_str(),
				samples.data(),
				static_cast<int>(samples.size()),
				1.0,
				0.0,
				spec);
		}

		ImPlot::EndPlot();
	}

	ImGui::End();
}

std::string_view CircuitLab::UI::ComponentValueToString(CircuitLab::ComponentValue value)
{
	switch (value)
	{
	case ComponentValue::resistance: return "Resistance";
	case ComponentValue::voltage:    return "Voltage";
	case ComponentValue::amplitude:  return "Amplitude";
	case ComponentValue::frequency:  return "Frequency";
	case ComponentValue::phase:      return "Phase";
	default:                         return "Unknown";
	}
}

float CircuitLab::UI::PointToStraightDistance(const sf::Vector2f &A, const sf::Vector2f &B, const sf::Vector2f &P)
{
	Vec2 AB(A.x - B.x, A.y - B.y);
	Vec2 AP(A.x - P.x, A.y - P.y);
	float dot = AB.x * AP.y - AP.x * AB.y;
	float distAB = std::sqrt((A.x - B.x) * (A.x - B.x) + (A.y - B.y) * (A.y - B.y));
	return std::abs(dot)/distAB;
}

int CircuitLab::UI::RemoveLinkFromNodeView(int nodeViewId, int linkViewId)
{
	for (auto &nv : m_nodeViewList)
	{
		LOG_DEBUG("linkViewId in list: " << nv.id);
		if (nv.id == nodeViewId)
		{
			nv.linkViewIds.erase(
				std::remove_if(nv.linkViewIds.begin(), nv.linkViewIds.end(),
					[linkViewId](const int id) {
						return id == linkViewId;
					}),
				nv.linkViewIds.end()
			);

			return static_cast<int>(nv.linkViewIds.size());
		}
	}
	return -1;
}

void CircuitLab::UI::CreateLinkViewCurrentList()
{
	m_linkViewCurrentList.clear();
	for (const auto &lv : m_linkViewList)
	{
		std::vector<int> termList = m_onGetCompTerminalId(lv.compIdA);
		if (termList.size() < 2)
		{
			m_linkViewCurrentList[lv.id] = 0.0;
			continue;
		}
		int nodeId1 = termList[0];
		int nodeId2 = termList[1];
		double current = m_simulationOutput.currentBranch[{nodeId1, nodeId2, lv.compIdA}];
		m_linkViewCurrentList[lv.id] = (lv.termIndexA == 0) ? -current : current;
	}
}

void CircuitLab::UI::CreateLinkParticlesList()
{
	m_linkParticlesList.clear();
	for (const auto &lv : m_linkViewList)
	{
		sf::Vector2f nodeViewPos = GetNodeviewPositionByNodeViewId(lv.nodeViewId);
		LinkPararticles newLinkParticles;
		float linkLenght = std::sqrt(((nodeViewPos.x - lv.startPos.x)* (nodeViewPos.x - lv.startPos.x)) + ((nodeViewPos.y - lv.startPos.y) * (nodeViewPos.y - lv.startPos.y)));
		newLinkParticles.linkViewId = lv.id;
		newLinkParticles.offset = 0.0f;
		newLinkParticles.count = static_cast<int>(linkLenght / (PARTICLE_SIZE * PARTICLE_SPACING_FACTOR));
		m_linkParticlesList.emplace_back(newLinkParticles);
	}
}

void CircuitLab::UI::UpdateParticles(float dt)
{
	for (auto &lp : m_linkParticlesList)
	{
		lp.offset = std::fmod(lp.offset + static_cast<float>(m_linkViewCurrentList[lp.linkViewId]) * dt * PARTICLE_SPEED_SCALE + 1.0f, 1.0f);
	}
}

CircuitLab::NodeView CircuitLab::UI::GetNodeViewFromLInkId(int linkViewId)
{
	for (auto &nv : m_nodeViewList)
		for (const auto &lv : nv.linkViewIds)
			if (lv == linkViewId)
				return nv;

	// non dovrebbe mai succedere se i dati sono consistenti
	throw std::runtime_error("NodeView not found for linkViewId: " + std::to_string(linkViewId));
}

sf::Vector2f CircuitLab::UI::GetNodeviewPositionByNodeViewId(int nodeViewId)
{
	for (const auto &nv : m_nodeViewList)
		if (nv.id == nodeViewId)
			return nv.position;
	
	throw std::runtime_error("NodeView not found for id: " + std::to_string(nodeViewId));
}

int CircuitLab::UI::GetNodeViewIdByTerminal(int compId, int termIndex) const
{
	for (const auto &lv : m_linkViewList)
		if (lv.compIdA == compId && lv.termIndexA == termIndex)
			return lv.nodeViewId;
	return -1;
}

void CircuitLab::UI::UpdateLinksForNodeView(int nodeViewId, sf::Vector2f newPos)
{
	for (auto &nv : m_nodeViewList)
		if (nv.id == nodeViewId)
		{
			nv.position = newPos;
			for (int lvId : nv.linkViewIds)
				for (auto &lv : m_linkViewList)
					if (lv.id == lvId)
						lv.targetPos = newPos;
			break;
		}
}

// Inizializza la finestra SFML e ImGui-SFML.
// Lancia un'eccezione se ImGui o il font non riescono ad inizializzarsi.
CircuitLab::UI::UI(unsigned int width, unsigned int heigth, const std::string &title) :
	m_width(width),
	m_heigth(heigth),
	m_title(title),
	m_window(sf::VideoMode({ m_width, m_heigth }), m_title),
	m_showOscilloscope(false)
{
	if (!ImGui::SFML::Init(m_window))
		throw std::runtime_error("Impossibile inizializzare ImGui-SFML");

	ImPlot::CreateContext();

	if (!m_font.openFromFile("JetBrainsMono-Regular.ttf"))
		throw std::runtime_error("Impossibile caricare il font");

	m_view = sf::View(sf::FloatRect({ 0.0f, 0.0f }, { static_cast<float>(m_width - PANEL_WIDTH), static_cast<float>(m_heigth) }));

	m_linkViewIdCount = 0;
	m_nodeViewCount = 0;
}

// Shutdown di ImGui-SFML alla distruzione della UI
CircuitLab::UI::~UI()
{
	ImPlot::DestroyContext();
	ImGui::SFML::Shutdown();
}

void CircuitLab::UI::AddViewComponent(int compId, const std::string &name, ComponentType type, Vec2 position, float rotation)
{
	m_componentViewList.emplace_back(ComponentView(compId, position, rotation, name, type));
}

int CircuitLab::UI::AddViewLink(int comp1, int term1, int nodeViewId)
{
	NodeView nv = GetNodeViewById(nodeViewId);
	LinkView newLink = GetLinkCoords(comp1, term1, nv);
	m_linkViewList.emplace_back(newLink);
	return newLink.id;
}

int CircuitLab::UI::AddNodeView(int nodeId, sf::Vector2f position)
{
	NodeView newNodeView;
	newNodeView.id = ++m_nodeViewCount;
	newNodeView.nodeId = nodeId;
	newNodeView.position = position;
	m_nodeViewList.push_back(newNodeView);
	return newNodeView.id;
}

void CircuitLab::UI::Clear()
{
	m_componentViewList.clear();
	m_linkViewList.clear();
	m_nodeViewList.clear();

	m_selectedComponent.compId = -1;
	m_selectedComponent.terminalIndex = -1;
	m_selectedComponent.linkId = -1;
	m_selectedComponent.state = SelectionState::none;
	m_selectedComponent.clickPos = sf::Vector2f(0.0f, 0.0f);
	m_linkViewIdCount = 0;
	m_nodeViewCount = 0;
}

void CircuitLab::UI::Render()
{
	sf::Time dt = m_deltaClock.restart();
	// --- Aggiornamento ImGui ---
	ImGui::SFML::Update(m_window, dt);

	DrawImageGuiPanel();

	// --- Rendering canvas ---
	m_window.clear(BACKGROUND_COLOR);

	m_view.setViewport(sf::FloatRect({ 0.f, 0.f }, { (static_cast<float>(m_width - PANEL_WIDTH) / m_width), 1.f }));
	m_window.setView(m_view);

	DrawComponents();

	DrawNodes();

	UpdateParticles(dt.asSeconds());

	DrawWires();

	m_window.setView(sf::View(sf::FloatRect({ 0.0f, 0.0f }, { static_cast<float>(m_width), static_cast<float>(m_heigth) })));

	// Render ImGui sopra il canvas
	ImGui::SFML::Render(m_window);
	m_window.display();
}