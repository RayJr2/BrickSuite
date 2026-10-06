/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#include "BuildRequirement.h"

int BuildRequirement::id() const { return m_id; }
void BuildRequirement::setId(int id) { m_id = id; }
int BuildRequirement::buildId() const { return m_buildId; }
void BuildRequirement::setBuildId(int buildId) { m_buildId = buildId; }
int BuildRequirement::partId() const { return m_partId; }
void BuildRequirement::setPartId(int partId) { m_partId = partId; }
int BuildRequirement::colorId() const { return m_colorId; }
void BuildRequirement::setColorId(int colorId) { m_colorId = colorId; }
int BuildRequirement::substitutePartId() const { return m_substitutePartId; }
void BuildRequirement::setSubstitutePartId(int partId) { m_substitutePartId = partId; }
int BuildRequirement::substituteColorId() const { return m_substituteColorId; }
void BuildRequirement::setSubstituteColorId(int colorId) { m_substituteColorId = colorId; }
int BuildRequirement::effectivePartId() const { return m_substitutePartId > 0 ? m_substitutePartId : m_partId; }
int BuildRequirement::effectiveColorId() const { return m_substituteColorId > 0 ? m_substituteColorId : m_colorId; }
int BuildRequirement::quantityRequired() const { return m_quantityRequired; }
void BuildRequirement::setQuantityRequired(int quantityRequired) { m_quantityRequired = quantityRequired; }
int BuildRequirement::quantityPulled() const { return m_quantityPulled; }
void BuildRequirement::setQuantityPulled(int quantityPulled) { m_quantityPulled = quantityPulled; }
int BuildRequirement::quantityReleased() const { return m_quantityReleased; }
void BuildRequirement::setQuantityReleased(int quantityReleased) { m_quantityReleased = quantityReleased; }
bool BuildRequirement::isSpare() const { return m_isSpare; }
void BuildRequirement::setIsSpare(bool isSpare) { m_isSpare = isSpare; }
QDateTime BuildRequirement::createdUtc() const { return m_createdUtc; }
void BuildRequirement::setCreatedUtc(const QDateTime& createdUtc) { m_createdUtc = createdUtc; }
QDateTime BuildRequirement::modifiedUtc() const { return m_modifiedUtc; }
void BuildRequirement::setModifiedUtc(const QDateTime& modifiedUtc) { m_modifiedUtc = modifiedUtc; }
