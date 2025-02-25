
```cpp
// Helper function declarations
void handleTwoVertexCase(const std::vector<PDCELVertex*>& base, int side, double dist,
                         std::vector<PDCELVertex*>& offset_vertices,
                         std::vector<int>& link_to_2,
                         std::vector<std::vector<int>>& id_pairs, Message* pmessage);

void processAllSegments(const std::vector<PDCELVertex*>& base, int side, double dist,
                        std::vector<PDCELVertex*>& vertices_tmp,
                        std::vector<int>& link_to_tmp, Message* pmessage);

void handleSegmentIntersection(PDCELVertex* v1_prev, PDCELVertex* v2_prev,
                               PDCELVertex* v1_tmp, PDCELVertex* v2_tmp,
                               std::vector<PDCELVertex*>& vertices_tmp,
                               Message* pmessage);

void groupValidSubLines(const std::vector<PDCELVertex*>& base,
                        const std::vector<PDCELVertex*>& vertices_tmp,
                        const std::vector<int>& link_to_tmp,
                        std::vector<std::vector<PDCELVertex*>>& lines_group,
                        std::vector<std::vector<int>>& link_tos_group,
                        Message* pmessage);

void trimAndConnectSublines(std::vector<std::vector<PDCELVertex*>>& lines_group,
                            std::vector<std::vector<int>>& link_tos_group,
                            std::vector<std::vector<PDCELVertex*>>& trimmed_sublines,
                            std::vector<std::vector<int>>& trimmed_link_to_base_indices,
                            Message* pmessage);

void processClosedCurveCase(const std::vector<PDCELVertex*>& base,
                            std::vector<std::vector<PDCELVertex*>>& lines_group,
                            std::vector<std::vector<int>>& link_tos_group,
                            std::vector<std::vector<PDCELVertex*>>& trimmed_sublines,
                            std::vector<std::vector<int>>& trimmed_link_to_base_indices,
                            Message* pmessage);

void buildOutputLinks(const std::vector<std::vector<PDCELVertex*>>& trimmed_sublines,
                      const std::vector<std::vector<int>>& trimmed_link_to_base_indices,
                      std::vector<PDCELVertex*>& offset_vertices,
                      std::vector<int>& link_to_2,
                      std::vector<std::vector<int>>& id_pairs,
                      Message* pmessage);

// Main function implementation
int offset(const std::vector<PDCELVertex*>& base, int side, double dist,
           std::vector<PDCELVertex*>& offset_vertices, std::vector<int>& link_to_2,
           std::vector<std::vector<int>>& id_pairs, Message* pmessage) {
    pmessage->increaseIndent();
    PLOG(debug) << pmessage->message("offsetting a polyline");

    if (base.size() == 2) {
        handleTwoVertexCase(base, side, dist, offset_vertices, link_to_2, id_pairs, pmessage);
        pmessage->decreaseIndent();
        return 1;
    }

    std::vector<PDCELVertex*> vertices_tmp;
    std::vector<int> link_to_tmp;
    processAllSegments(base, side, dist, vertices_tmp, link_to_tmp, pmessage);

    std::vector<std::vector<PDCELVertex*>> lines_group;
    std::vector<std::vector<int>> link_tos_group;
    groupValidSubLines(base, vertices_tmp, link_to_tmp, lines_group, link_tos_group, pmessage);

    std::vector<std::vector<PDCELVertex*>> trimmed_sublines;
    std::vector<std::vector<int>> trimmed_link_to_base_indices;
    if (lines_group.size() > 1) {
        trimAndConnectSublines(lines_group, link_tos_group, trimmed_sublines,
                               trimmed_link_to_base_indices, pmessage);
    } else {
        trimmed_sublines = lines_group;
        trimmed_link_to_base_indices = link_tos_group;
    }

    if (!base.empty() && base.front() == base.back()) {
        processClosedCurveCase(base, lines_group, link_tos_group, trimmed_sublines,
                               trimmed_link_to_base_indices, pmessage);
    }

    buildOutputLinks(trimmed_sublines, trimmed_link_to_base_indices,
                     offset_vertices, link_to_2, id_pairs, pmessage);

    pmessage->decreaseIndent();
    return 1;
}

// Helper function implementations
void handleTwoVertexCase(const std::vector<PDCELVertex*>& base, int side, double dist,
                         std::vector<PDCELVertex*>& offset_vertices,
                         std::vector<int>& link_to_2,
                         std::vector<std::vector<int>>& id_pairs, Message* pmessage) {
    PLOG(debug) << pmessage->message("the base curve has only two vertices");
    PDCELVertex* v1_tmp = new PDCELVertex();
    PDCELVertex* v2_tmp = new PDCELVertex();

    offset(base[0], base[1], side, dist, v1_tmp, v2_tmp);

    offset_vertices.push_back(v1_tmp);
    offset_vertices.push_back(v2_tmp);

    link_to_2.push_back(0);
    link_to_2.push_back(1);

    id_pairs.push_back({0, 0});
    id_pairs.push_back({1, 1});
}

void processAllSegments(const std::vector<PDCELVertex*>& base, int side, double dist,
                        std::vector<PDCELVertex*>& vertices_tmp,
                        std::vector<int>& link_to_tmp, Message* pmessage) {
    PDCELVertex *v1_prev = nullptr, *v2_prev = nullptr;
    for (int i = 0; i < base.size() - 1; ++i) {
        PDCELVertex* v1_tmp = new PDCELVertex();
        PDCELVertex* v2_tmp = new PDCELVertex();

        offset(base[i], base[i + 1], side, dist, v1_tmp, v2_tmp);
        link_to_tmp.push_back(i);

        if (i == 0) {
            vertices_tmp.push_back(v1_tmp);
        } else {
            handleSegmentIntersection(v1_prev, v2_prev, v1_tmp, v2_tmp, vertices_tmp, pmessage);
        }

        if (i == base.size() - 2) {
            vertices_tmp.push_back(v2_tmp);
            link_to_tmp.push_back(i + 1);
        }

        v1_prev = v1_tmp;
        v2_prev = v2_tmp;
    }
}

void handleSegmentIntersection(PDCELVertex* v1_prev, PDCELVertex* v2_prev,
                               PDCELVertex* v1_tmp, PDCELVertex* v2_tmp,
                               std::vector<PDCELVertex*>& vertices_tmp,
                               Message* pmessage) {
    PLOG(debug) << pmessage->message("calculate intersection");
    h2d::Point2d p1_prev(v1_prev->point2()[0], v1_prev->point2()[1]);
    h2d::Point2d p2_prev(v2_prev->point2()[0], v2_prev->point2()[1]);
    h2d::Point2d p1_tmp(v1_tmp->point2()[0], v1_tmp->point2()[1]);
    h2d::Point2d p2_tmp(v2_tmp->point2()[0], v2_tmp->point2()[1]);

    h2d::Segment seg_prev(p1_prev, p2_prev);
    h2d::Segment seg_curr(p1_tmp, p2_tmp);
    auto res = seg_prev.intersects(seg_curr);

    PDCELVertex* v_new = nullptr;
    if (res()) {
        auto pts = res.get();
        if (isClose(pts.getX(), pts.getY(), p1_prev.getX(), p1_prev.getY(), ABS_TOL, REL_TOL)) {
            v_new = v1_prev;
        } else if (isClose(pts.getX(), pts.getY(), p2_prev.getX(), p2_prev.getY(), ABS_TOL, REL_TOL)) {
            v_new = v2_prev;
        } else {
            v_new = new PDCELVertex(0, pts.getX(), pts.getY());
        }
    } else {
        v_new = v2_prev;
    }

    vertices_tmp.push_back(v_new);
}

void groupValidSubLines(const std::vector<PDCELVertex*>& base,
                        const std::vector<PDCELVertex*>& vertices_tmp,
                        const std::vector<int>& link_to_tmp,
                        std::vector<std::vector<PDCELVertex*>>& lines_group,
                        std::vector<std::vector<int>>& link_tos_group,
                        Message* pmessage) {
    SVector3 vec_base, vec_off;
    bool check_prev = false;
    for (int j = 0; j < base.size() - 1; ++j) {
        vec_base = SVector3(base[j]->point(), base[j + 1]->point());
        vec_off = SVector3(vertices_tmp[j]->point(), vertices_tmp[j + 1]->point());

        bool check_next = (dot(vec_base, vec_off) > 0) && (vec_off.normSq() >= TOLERANCE * TOLERANCE);

        if (check_next) {
            if (!check_prev) {
                lines_group.push_back({vertices_tmp[j]});
                link_tos_group.push_back({link_to_tmp[j]});
            }
            lines_group.back().push_back(vertices_tmp[j + 1]);
            link_tos_group.back().push_back(link_to_tmp[j + 1]);
        }
        check_prev = check_next;
    }
}

void trimAndConnectSublines(std::vector<std::vector<PDCELVertex*>>& lines_group,
                            std::vector<std::vector<int>>& link_tos_group,
                            std::vector<std::vector<PDCELVertex*>>& trimmed_sublines,
                            std::vector<std::vector<int>>& trimmed_link_to_base_indices,
                            Message* pmessage) {
    for (int line_i = 0; line_i < lines_group.size() - 1; ++line_i) {
        auto& sline_i = lines_group[line_i];
        auto& sline_i1 = lines_group[line_i + 1];
        auto& link_i = link_tos_group[line_i];
        auto& link_i1 = link_tos_group[line_i + 1];

        // Find intersections and trim sublines
        std::vector<int> i1s, i2s;
        std::vector<double> u1s, u2s;
        findAllIntersections(sline_i, sline_i1, i1s, i2s, u1s, u2s);

        int ls_i1, j1;
        double ls_u1 = getIntersectionLocation(sline_i, i1s, u1s, 1, 0, ls_i1, j1, pmessage);
        int ls_i2 = i2s[j1];
        double ls_u2 = u2s[j1];

        int is_new_1, is_new_2;
        PDCELVertex* v0 = getIntersectionVertex(sline_i, sline_i1, ls_i1, ls_i2, ls_u1, ls_u2,
                                                1, 0, 0, 0, is_new_1, is_new_2, TOLERANCE);

        trim(sline_i, v0, 1);
        trim(sline_i1, v0, 0);

        // Adjust link indices
        link_tos_group[line_i].resize(ls_i1 + 1);
        link_tos_group[line_i + 1].erase(link_tos_group[line_i + 1].begin(),
                                         link_tos_group[line_i + 1].begin() + ls_i2 - 1);

        trimmed_sublines.push_back(sline_i);
        trimmed_link_to_base_indices.push_back(link_tos_group[line_i]);
    }
    trimmed_sublines.push_back(lines_group.back());
    trimmed_link_to_base_indices.push_back(link_tos_group.back());
}

void processClosedCurveCase(const std::vector<PDCELVertex*>& base,
                            std::vector<std::vector<PDCELVertex*>>& lines_group,
                            std::vector<std::vector<int>>& link_tos_group,
                            std::vector<std::vector<PDCELVertex*>>& trimmed_sublines,
                            std::vector<std::vector<int>>& trimmed_link_to_base_indices,
                            Message* pmessage) {
    auto& sline_last = lines_group.back();
    auto& sline_first = lines_group.front();
    auto& link_last = link_tos_group.back();
    auto& link_first = link_tos_group.front();

    // Find intersections between last and first sublines
    std::vector<int> i1s, i2s;
    std::vector<double> u1s, u2s;
    findAllIntersections(sline_last, sline_first, i1s, i2s, u1s, u2s);

    int ls_i1, j1;
    double ls_u1 = getIntersectionLocation(sline_last, i1s, u1s, 1, 0, ls_i1, j1, pmessage);
    int ls_i2 = i2s[j1];
    double ls_u2 = u2s[j1];

    int is_new_1, is_new_2;
    PDCELVertex* v0 = getIntersectionVertex(sline_last, sline_first, ls_i1, ls_i2, ls_u1, ls_u2,
                                            1, 0, 0, 0, is_new_1, is_new_2, TOLERANCE);

    trim(sline_last, v0, 1);
    trim(sline_first, v0, 0);

    // Adjust link indices
    link_tos_group.back().resize(ls_i1 + 1);
    link_tos_group.front().erase(link_tos_group.front().begin(),
                                 link_tos_group.front().begin() + ls_i2 - 1);

    // Update trimmed sublines
    if (!trimmed_sublines.empty()) {
        trimmed_sublines.back() = sline_last;
        trimmed_sublines.front() = sline_first;
        trimmed_link_to_base_indices.back() = link_tos_group.back();
        trimmed_link_to_base_indices.front() = link_tos_group.front();
    }
}

void buildOutputLinks(const std::vector<std::vector<PDCELVertex*>>& trimmed_sublines,
                      const std::vector<std::vector<int>>& trimmed_link_to_base_indices,
                      std::vector<PDCELVertex*>& offset_vertices,
                      std::vector<int>& link_to_2,
                      std::vector<std::vector<int>>& id_pairs,
                      Message* pmessage) {
    link_to_2.resize(trimmed_link_to_base_indices.size(), 0);
    for (size_t i = 0; i < trimmed_sublines.size(); ++i) {
        const auto& subline = trimmed_sublines[i];
        const auto& link_indices = trimmed_link_to_base_indices[i];
        for (size_t j = 0; j < subline.size() - 1; ++j) {
            offset_vertices.push_back(subline[j]);
            link_to_2[link_indices[j]] = offset_vertices.size() - 1;
        }
    }
    offset_vertices.push_back(trimmed_sublines.back().back());

    for (size_t i = 0; i < link_to_2.size(); ++i) {
        if (i > 0 && link_to_2[i] == 0) {
            link_to_2[i] = link_to_2[i - 1];
        }
        id_pairs.push_back({static_cast<int>(i), link_to_2[i]});
    }
}
```
