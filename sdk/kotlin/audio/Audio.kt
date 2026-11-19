package audio

object Audio {
    fun play() {}
    fun pause() {}
    fun skip() {}
    fun stop(playingId: String = "", stateId: String = "") {}
    fun status(widgetId: String) {}
}
