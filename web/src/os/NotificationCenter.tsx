/**
 * Literal React port of Chat-Room `resources/js/os/shell/NotificationCenter.vue`
 * (empty/local state — no notifications store in this app)
 */
type Props = {
  onActivate?: (item: unknown) => void;
};

export function NotificationCenter(_props: Props) {
  const unreadCount = 0;
  const loading = false;
  const error: string | null = null;
  const notifications: unknown[] = [];
  const groupedNotifications: Array<{
    id: string;
    label: string;
    items: unknown[];
  }> = [];

  return (
    <section className="tray-notifications" aria-label="Notifications">
      <header className="tray-notifications-header">
        <h2 className="tray-section-title">Notifications</h2>
        <button
          type="button"
          className="tray-mark-all"
          disabled={!unreadCount}
        >
          Mark all as read
        </button>
      </header>

      {error ? <p className="tray-notifications-error">{error}</p> : null}

      {loading && !notifications.length ? (
        <div className="tray-notifications-loading" aria-busy="true">
          {[1, 2, 3].map((n) => (
            <div key={n} className="tray-notification-skeleton" />
          ))}
        </div>
      ) : groupedNotifications.length ? (
        <div className="tray-notifications-scroll os-scroll">
          {groupedNotifications.map((group) => (
            <div key={group.id} className="tray-notification-group">
              <h3 className="tray-notification-group-label">{group.label}</h3>
              <div className="tray-notification-group-list" />
            </div>
          ))}
        </div>
      ) : (
        <div className="tray-notifications-empty">
          <p className="tray-notifications-empty-title">All caught up</p>
          <p className="tray-notifications-empty-copy">No new notifications.</p>
        </div>
      )}
    </section>
  );
}
