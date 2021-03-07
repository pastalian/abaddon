#include "rolespane.hpp"
#include "../../abaddon.hpp"

GuildSettingsRolesPane::GuildSettingsRolesPane(Snowflake id)
    : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL)
    , GuildID(id)
    , m_roles_list(id)
    , m_roles_perms(id) {
    set_name("guild-roles-pane");
    set_hexpand(true);
    set_vexpand(true);

    m_roles_list.signal_role_select().connect(sigc::mem_fun(*this, &GuildSettingsRolesPane::OnRoleSelect));

    m_roles_perms.set_sensitive(false);

    m_layout.set_homogeneous(true);
    m_layout.add(m_roles_list);
    m_layout.add(m_roles_perms);
    add(m_layout);

    m_roles_list.show();
    m_roles_perms.show();
    m_layout.show();
}

void GuildSettingsRolesPane::OnRoleSelect(Snowflake role_id) {
    auto &discord = Abaddon::Get().GetDiscordClient();
    const auto role = *discord.GetRole(role_id);
    m_roles_perms.SetRole(role);

    const auto self_id = discord.GetUserData().ID;

    const auto is_owner = discord.GetGuild(GuildID)->OwnerID == self_id;
    if (is_owner)
        m_roles_perms.set_sensitive(true);
    else {
        const bool can_modify = discord.HasGuildPermission(self_id, GuildID, Permission::MANAGE_ROLES);
        if (!can_modify) {
            m_roles_perms.set_sensitive(false);
        } else {
            const auto highest = discord.GetMemberHighestRole(GuildID, self_id);
            m_roles_perms.set_sensitive(highest.has_value() && highest->Position > role.Position);
        }
    }
}

GuildSettingsRolesPaneRoles::GuildSettingsRolesPaneRoles(Snowflake guild_id)
    : Gtk::Box(Gtk::ORIENTATION_VERTICAL)
    , GuildID(guild_id) {
    m_list.get_style_context()->add_class("guild-roles-pane-list");

    m_list_scroll.set_hexpand(true);
    m_list_scroll.set_vexpand(true);
    m_list_scroll.set_propagate_natural_height(true);

    m_list.set_selection_mode(Gtk::SELECTION_SINGLE);
    m_list.signal_row_selected().connect([this](Gtk::ListBoxRow *selected_) {
        if (auto *selected = dynamic_cast<GuildSettingsRolesPaneRolesListItem *>(selected_))
            m_signal_role_select.emit(selected->RoleID);
    });

    auto &discord = Abaddon::Get().GetDiscordClient();
    discord.signal_role_create().connect(sigc::mem_fun(*this, &GuildSettingsRolesPaneRoles::OnRoleCreate));
    //discord.signal_role_update().connect(sigc::mem_fun(*this, &GuildSettingsRolesPaneRoles::OnRoleUpdate));
    discord.signal_role_delete().connect(sigc::mem_fun(*this, &GuildSettingsRolesPaneRoles::OnRoleDelete));

    const auto guild = *discord.GetGuild(GuildID);
    const auto roles = guild.FetchRoles();
    for (const auto &role : roles) {
        auto *row = Gtk::manage(new GuildSettingsRolesPaneRolesListItem(guild, role));
        row->set_margin_start(5);
        row->set_halign(Gtk::ALIGN_FILL);
        row->show();
        m_rows[role.ID] = row;
        m_list.add(*row);
    }

    m_list.set_sort_func([this](Gtk::ListBoxRow *rowa_, Gtk::ListBoxRow *rowb_) -> int {
        auto *rowa = dynamic_cast<GuildSettingsRolesPaneRolesListItem *>(rowa_);
        auto *rowb = dynamic_cast<GuildSettingsRolesPaneRolesListItem *>(rowb_);
        return rowb->Position - rowa->Position;
    });
    m_list.invalidate_sort();

    m_list.set_filter_func([this](Gtk::ListBoxRow *row_) -> bool {
        const auto search_term = m_search.get_text();
        if (search_term.size() == 0) return true;
        if (auto *row = dynamic_cast<GuildSettingsRolesPaneRolesListItem *>(row_))
            return StringContainsCaseless(row->DisplayTerm, m_search.get_text());
        return true;
    });

    m_search.set_placeholder_text("Filter");
    m_search.signal_changed().connect([this] {
        m_list.invalidate_filter();
    });

    m_list_scroll.add(m_list);
    add(m_search);
    add(m_list_scroll);

    m_search.show();
    m_list.show();
    m_list_scroll.show();
}

void GuildSettingsRolesPaneRoles::OnRoleCreate(Snowflake guild_id, Snowflake role_id) {
    if (guild_id != GuildID) return;
    auto &discord = Abaddon::Get().GetDiscordClient();
    const auto guild = *discord.GetGuild(guild_id);
    const auto role = *discord.GetRole(role_id);
    auto *row = Gtk::manage(new GuildSettingsRolesPaneRolesListItem(guild, role));
    row->show();
    m_rows[role_id] = row;
    m_list.add(*row);
}

void GuildSettingsRolesPaneRoles::OnRoleUpdate(Snowflake guild_id, Snowflake role_id) {
    if (guild_id != GuildID) return;
    m_list.invalidate_filter();
    m_list.invalidate_sort();
}

void GuildSettingsRolesPaneRoles::OnRoleDelete(Snowflake guild_id, Snowflake role_id) {
    if (guild_id != GuildID) return;
    auto it = m_rows.find(role_id);
    delete it->second;
    m_rows.erase(it);
}

GuildSettingsRolesPaneRoles::type_signal_role_select GuildSettingsRolesPaneRoles::signal_role_select() {
    return m_signal_role_select;
}

GuildSettingsRolesPaneRolesListItem::GuildSettingsRolesPaneRolesListItem(const GuildData &guild, const RoleData &role)
    : GuildID(guild.ID)
    , RoleID(role.ID)
    , Position(role.Position) {
    auto &discord = Abaddon::Get().GetDiscordClient();

    set_hexpand(true);

    UpdateItem(role);

    discord.signal_role_update().connect([this, &discord](Snowflake guild_id, Snowflake role_id) {
        if (guild_id != GuildID || role_id != RoleID) return;
        const auto role = discord.GetRole(RoleID);
        if (!role.has_value())
            printf("nullopt??? %llu\n", (uint64_t)RoleID);
        Position = role->Position;
        UpdateItem(*role);
        changed();
    });

    m_ev.set_halign(Gtk::ALIGN_START);
    m_ev.add(m_name);
    add(m_ev);

    m_name.show();
    m_ev.show();
}

void GuildSettingsRolesPaneRolesListItem::UpdateItem(const RoleData &role) {
    DisplayTerm = role.Name;

    if (role.Color != 0)
        m_name.set_markup("<span color='#" + IntToCSSColor(role.Color) + "'>" +
                          Glib::Markup::escape_text(role.Name) +
                          "</span>");
    else
        m_name.set_text(role.Name);
}

GuildSettingsRolesPaneInfo::GuildSettingsRolesPaneInfo(Snowflake guild_id)
    : GuildID(guild_id) {
    set_propagate_natural_height(true);
    set_propagate_natural_width(true);

    Abaddon::Get().GetDiscordClient().signal_role_update().connect(sigc::mem_fun(*this, &GuildSettingsRolesPaneInfo::OnRoleUpdate));

    int left_ypos = 0;
    int right_ypos = 0;

    const int LEFT = 0;
    const int RIGHT = 1;

    auto add_perms = [&](const std::string &label, int side, const std::initializer_list<Permission> &perms) {
        int &pos = side == LEFT ? left_ypos : right_ypos;
        auto *header = Gtk::manage(new Gtk::Label(label));
        header->show();
        m_grid.attach(*header, side, pos++, 1, 1);
        for (const auto perm : perms) {
            auto *btn = Gtk::manage(new GuildSettingsRolesPanePermItem(perm));
            btn->signal_permission_click().connect(sigc::mem_fun(*this, &GuildSettingsRolesPaneInfo::OnPermissionToggle));
            m_perm_items[perm] = btn;
            btn->show();
            m_grid.attach(*btn, side, pos++, 1, 1);
        }
        pos++;
    };

    // fuck you clang-format you suck
    // clang-format off
    add_perms("General", LEFT, { 
        Permission::VIEW_CHANNEL,
        Permission::MANAGE_CHANNELS,
        Permission::MANAGE_ROLES,
        Permission::MANAGE_EMOJIS,
        Permission::VIEW_AUDIT_LOG,
        Permission::MANAGE_WEBHOOKS,
        Permission::MANAGE_GUILD });

    add_perms("Membership", LEFT, {
        Permission::CREATE_INSTANT_INVITE,
        Permission::CHANGE_NICKNAME,
        Permission::MANAGE_NICKNAMES,
        Permission::KICK_MEMBERS,
        Permission::BAN_MEMBERS });

    add_perms("Text Channels", RIGHT, {
        Permission::SEND_MESSAGES,
        Permission::EMBED_LINKS,
        Permission::ATTACH_FILES,
        Permission::ADD_REACTIONS,
        Permission::USE_EXTERNAL_EMOJIS,
        Permission::MENTION_EVERYONE,
        Permission::MANAGE_MESSAGES,
        Permission::READ_MESSAGE_HISTORY,
        Permission::SEND_TTS_MESSAGES,
        Permission::USE_APPLICATION_COMMANDS });

    add_perms("Voice Channels", RIGHT, {
        Permission::CONNECT,
        Permission::SPEAK,
        Permission::STREAM,
        Permission::USE_VAD,
        Permission::PRIORITY_SPEAKER,
        Permission::MUTE_MEMBERS,
        Permission::DEAFEN_MEMBERS,
        Permission::MOVE_MEMBERS });

    add_perms("Advanced", LEFT, { Permission::ADMINISTRATOR });

    // clang-format on

    add(m_grid);
    m_grid.show();
}

void GuildSettingsRolesPaneInfo::SetRole(const RoleData &role) {
    for (auto it = m_update_connections.begin(); it != m_update_connections.end();) {
        it->disconnect();
        it = m_update_connections.erase(it);
    }

    RoleID = role.ID;
    m_perms = role.Permissions;
    for (const auto [perm, btn] : m_perm_items) {
        btn->set_sensitive(true);
        btn->set_active((role.Permissions & perm) == perm);
    }
}

void GuildSettingsRolesPaneInfo::OnRoleUpdate(Snowflake guild_id, Snowflake role_id) {
    if (guild_id != GuildID || role_id != RoleID) return;
    const auto role = *Abaddon::Get().GetDiscordClient().GetRole(RoleID);
    m_perms = role.Permissions;
    for (const auto [perm, btn] : m_perm_items)
        btn->set_active((role.Permissions & perm) == perm);
}

void GuildSettingsRolesPaneInfo::OnPermissionToggle(Permission perm, bool new_set) {
    auto btn = m_perm_items.at(perm);
    btn->set_sensitive(false);
    auto &discord = Abaddon::Get().GetDiscordClient();
    auto cb = [this, new_set, perm, btn](bool success) {
        if (!success) { // undo
            if (new_set)
                m_perms &= ~perm;
            else
                m_perms |= perm;
        } else
            btn->set_active(new_set);
        btn->set_sensitive(true);
    };

    if (new_set)
        m_perms |= perm;
    else
        m_perms &= ~perm;

    sigc::signal<void, bool> tmp;
    m_update_connections.push_back(tmp.connect(std::move(cb)));
    const auto tmp_cb = [this, tmp = std::move(tmp)](bool success) { tmp.emit(success); };
    discord.ModifyRolePermissions(GuildID, RoleID, m_perms, sigc::track_obj(tmp_cb, *this));
}

GuildSettingsRolesPanePermItem::GuildSettingsRolesPanePermItem(Permission perm)
    : Gtk::CheckButton(GetPermissionString(perm))
    , m_permission(perm) {
    set_tooltip_text(GetPermissionDescription(m_permission));

    const auto cb = [this](GdkEventButton *event) -> bool {
        if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_PRIMARY) {
            m_signal_permission.emit(m_permission, !get_active());
            return true;
        }
        return false;
    };
    signal_button_press_event().connect(cb, false);
}

GuildSettingsRolesPanePermItem::type_signal_permission_click GuildSettingsRolesPanePermItem::signal_permission_click() {
    return m_signal_permission;
}
