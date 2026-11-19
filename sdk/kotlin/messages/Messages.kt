package messages

object Messages {
    fun open() {}
    fun openThread(index: Int) {}
    fun compose() {}
    fun send() {}
    fun send(body: String, statusId: String = "") {}
}
