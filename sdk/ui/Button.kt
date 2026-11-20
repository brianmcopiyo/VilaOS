class Button(
    val id: String = "",
    val content: String = "",
    val background: String = "",
    val foreground: String = "",
    val borderBrush: String = "",
    val borderThickness: Int = 0,
    val cornerRadius: Int = 0,
    val padding: String = "",
    val variant: String = "primary",
    val icon: String = "",
    val iconSet: String = "lucide",
    val onClick: () -> Unit = {}
) {
    fun attach(screen: Screen) {}
}
