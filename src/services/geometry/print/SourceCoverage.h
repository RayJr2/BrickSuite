#pragma once

#include <QVector>
#include <QString>

namespace PrintGeometry {

enum class SourceCoverageRoute { None, SemanticOperands, FullSourceSolidification };
enum class SourceGroupCoverage { Unrepresented, SemanticOperand, FullSource, IntentionallyExcluded };

struct SourceCoverageGroup {
    int index = -1;
    int triangleCount = 0;
    SourceGroupCoverage status = SourceGroupCoverage::Unrepresented;
    QString exclusionReason;
};

struct SourceCoverage {
    // Tracks authoritative input ownership, not triangle identity after MCUT
    // or a volumetric resample. Final manifold and fidelity checks remain separate.
    SourceCoverageRoute route = SourceCoverageRoute::None;
    int expandedTriangleCount = 0;
    int stitchedTriangleCount = 0;
    int groupedTriangleCount = 0;
    int degenerateTriangleCount = 0;
    QString degenerateExclusionRule = QStringLiteral("Zero-area triangle omitted by the established welded-source rule");
    QVector<int> expandedTriangleForStitchedTriangle;
    QVector<int> groupForStitchedTriangle;
    QVector<SourceCoverageGroup> groups;

    int representedGroups() const {
        int count = 0;
        for (const auto& group : groups)
            if (group.status == SourceGroupCoverage::SemanticOperand ||
                group.status == SourceGroupCoverage::FullSource) ++count;
        return count;
    }
    int excludedGroups() const {
        int count = 0;
        for (const auto& group : groups)
            if (group.status == SourceGroupCoverage::IntentionallyExcluded) ++count;
        return count;
    }
    QVector<int> uncoveredGroups() const {
        QVector<int> result;
        for (const auto& group : groups)
            if (group.status == SourceGroupCoverage::Unrepresented) result.push_back(group.index);
        return result;
    }
    bool complete() const {
        if(route==SourceCoverageRoute::None || groups.isEmpty() || !uncoveredGroups().isEmpty() ||
           groupedTriangleCount + degenerateTriangleCount != stitchedTriangleCount ||
           expandedTriangleForStitchedTriangle.size() != stitchedTriangleCount ||
           groupForStitchedTriangle.size() != stitchedTriangleCount) return false;
        QVector<bool> expandedSeen(expandedTriangleCount, false);
        QVector<int> groupedCounts(groups.size(),0);
        int excluded = 0;
        for(int i=0;i<stitchedTriangleCount;++i){
            const int expanded=expandedTriangleForStitchedTriangle[i];
            const int group=groupForStitchedTriangle[i];
            if(expanded<0 || expanded>=expandedTriangleCount || group>=groups.size())return false;
            if(group<0){++excluded;if(group!=-1 || degenerateExclusionRule.isEmpty())return false;}
            else {
                if(groups[group].status==SourceGroupCoverage::Unrepresented || groups[group].index!=group ||
                   (route==SourceCoverageRoute::SemanticOperands && groups[group].status==SourceGroupCoverage::FullSource) ||
                   (route==SourceCoverageRoute::FullSourceSolidification && groups[group].status==SourceGroupCoverage::SemanticOperand) ||
                   (groups[group].status==SourceGroupCoverage::IntentionallyExcluded &&
                    groups[group].exclusionReason.isEmpty()))return false;
                ++groupedCounts[group];
            }
            expandedSeen[expanded]=true;
        }
        if(excluded!=degenerateTriangleCount)return false;
        for(int i=0;i<groups.size();++i)if(groupedCounts[i]!=groups[i].triangleCount)return false;
        for(bool seen:expandedSeen)if(!seen)return false;
        return true;
    }
};

} // namespace PrintGeometry
