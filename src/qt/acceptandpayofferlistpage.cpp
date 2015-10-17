#include "acceptandpayofferlistpage.h"
#include "ui_acceptandpayofferlistpage.h"
#include "init.h"
#include "util.h"
#include "offeracceptdialog.h"

#include "offer.h"

#include "bitcoingui.h"
#include "bitcoinrpc.h"

#include "guiutil.h"

#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>
#include <QString>
#if QT_VERSION >= 0x050000
#include <QUrlQuery>
#else
#include <QUrl>
#endif

using namespace std;
using namespace json_spirit;
extern const CRPCTable tableRPC;
extern string JSONRPCReply(const Value& result, const Value& error, const Value& id);
extern int64 convertCurrencyCodeToSyscoin(const vector<unsigned char> &vchCurrencyCode, const double &nPrice, const unsigned int &nHeight, int &precision);
extern int nBestHeight;
AcceptandPayOfferListPage::AcceptandPayOfferListPage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AcceptandPayOfferListPage)
{
    ui->setupUi(this);
	this->offerPaid = false;
	this->URIHandled = false;	
    ui->labelExplanation->setText(tr("Purchase an offer, Syscoin will be used from your balance to complete the transaction"));
    connect(ui->acceptButton, SIGNAL(clicked()), this, SLOT(acceptOffer()));
	connect(ui->lookupButton, SIGNAL(clicked()), this, SLOT(lookup()));
	connect(ui->offeridEdit, SIGNAL(textChanged(const QString &)), this, SLOT(resetState()));
	ui->notesEdit->setStyleSheet("color: rgb(0, 0, 0); background-color: rgb(255, 255, 255)");
}

AcceptandPayOfferListPage::~AcceptandPayOfferListPage()
{
    delete ui;
	this->URIHandled = false;
}
void AcceptandPayOfferListPage::resetState()
{
		this->offerPaid = false;
		this->URIHandled = false;
		updateCaption();
}
void AcceptandPayOfferListPage::updateCaption()
{
		
		if(this->offerPaid)
		{
			ui->labelExplanation->setText(tr("<font color='green'>You have successfully paid for this offer!</font>"));
		}
		else
		{
			ui->labelExplanation->setText(tr("Purchase this offer, Syscoin will be used from your balance to complete the transaction"));
		}
		
}
void AcceptandPayOfferListPage::OpenPayDialog()
{
	string currencyCode = ui->infoCurrency->text().toStdString();
	int precision;
	int64 iPrice = convertCurrencyCodeToSyscoin(vchFromString(currencyCode), ui->infoPrice->text().toDouble(), nBestHeight, precision);
	QString price = QString::number(ValueFromAmount(iPrice).get_real()*ui->qtyEdit->text().toLong());
	OfferAcceptDialog dlg(ui->infoTitle->text(), price, ui->qtyEdit->text(), ui->offeridEdit->text(), ui->notesEdit->toPlainText(), this);
	if(dlg.exec())
	{
		this->offerPaid = dlg.getPaymentStatus();
		if(this->offerPaid)
		{
			COffer offer;
			setValue(offer);
		}
	}
	updateCaption();
}
// send offeraccept with offer guid/qty as params and then send offerpay with wtxid (first param of response) as param, using RPC commands.
void AcceptandPayOfferListPage::acceptOffer()
{
	if(ui->qtyEdit->text().toInt() <= 0)
	{
		QMessageBox::critical(this, windowTitle(),
			tr("Invalid quantity when trying to accept this offer!"),
			QMessageBox::Ok, QMessageBox::Ok);
		return;
	}
	this->offerPaid = false;
	ui->labelExplanation->setText(tr("Waiting for confirmation on the purchase of this offer"));
	OpenPayDialog();
}

bool AcceptandPayOfferListPage::lookup(QString id)
{
	if(id == QString(""))
	{
		id = ui->offeridEdit->text();
	}
	string strError;
	string strMethod = string("offerinfo");
	Array params;
	Value result;
	params.push_back(id.toStdString());

    try {
        result = tableRPC.execute(strMethod, params);

		if (result.type() == obj_type)
		{
			Object offerObj;
			offerObj = result.get_obj();
			COffer offerOut;
			offerOut.vchRand = vchFromString(find_value(offerObj, "offer").get_str());
			offerOut.sTitle = vchFromString(find_value(offerObj, "title").get_str());
			offerOut.sCategory = vchFromString(find_value(offerObj, "category").get_str());
			offerOut.sCurrencyCode = vchFromString(find_value(offerObj, "currency").get_str());
			offerOut.SetPrice(QString::fromStdString(find_value(offerObj, "price").get_str()).toFloat());
			offerOut.nQty = QString::fromStdString(find_value(offerObj, "quantity").get_str()).toLong();	
			string descString = find_value(offerObj, "description").get_str();
			offerOut.sDescription = vchFromString(descString);
			Value outerDescValue;
			if (read_string(descString, outerDescValue))
			{
				if(outerDescValue.type() == obj_type)
				{
					const Object& outerDescObj = outerDescValue.get_obj();
					Value descValue = find_value(outerDescObj, "description");
					if (descValue.type() == str_type)
					{
						offerOut.sDescription = vchFromString(descValue.get_str());
					}
				}

			}
			offerOut.vchPaymentAddress = vchFromString(find_value(offerObj, "payment_address").get_str());
			setValue(offerOut);
			return true;
		}
		 

	}
	catch (Object& objError)
	{
		QMessageBox::critical(this, windowTitle(),
			tr("Could not find this offer, please check the offer ID and that it has been confirmed by the blockchain: ") + id,
				QMessageBox::Ok, QMessageBox::Ok);

	}
	catch(std::exception& e)
	{
		QMessageBox::critical(this, windowTitle(),
			tr("There was an exception trying to locate this offer, please check the offer ID and that it has been confirmed by the blockchain: ") + QString::fromStdString(e.what()),
				QMessageBox::Ok, QMessageBox::Ok);
	}
	return false;


}
bool AcceptandPayOfferListPage::handleURI(const QUrl &uri)
{	
	if(!uri.isValid() || uri.scheme() != QString("syscoin"))
	{
		QMessageBox::critical(this, windowTitle(),
		tr("URI Path is not valid: \"%1\"").arg(uri.path()),
			QMessageBox::Ok, QMessageBox::Ok);
	}
	if(this->URIHandled)
	{
		QMessageBox::critical(this, windowTitle(),
		tr("URI has been already handled"),
			QMessageBox::Ok, QMessageBox::Ok);
	}
	  // return if URI is not valid or is no Sys URI
    if(!uri.isValid() || uri.scheme() != QString("syscoin") || this->URIHandled)
        return false;
	bool foundQtyParam = false;
	string strError;	
    try {
		ui->notesEdit->setPlainText(QString(""));
		ui->qtyEdit->setText(QString("1"));
	#if QT_VERSION < 0x050000
		QList<QPair<QString, QString> > items = uri.queryItems();
	#else
		QUrlQuery uriQuery(uri);
		QList<QPair<QString, QString> > items = uriQuery.queryItems();
	#endif
		long qty = 0;
		for (QList<QPair<QString, QString> >::iterator i = items.begin(); i != items.end(); i++)
		{
			
			if (i->first == "quantity" || i->first == "qty")
			{
				qty = i->second.toLong();
				ui->qtyEdit->setText(QString::number(qty));
				foundQtyParam = true;
			}
			else if (i->first == "notes")
			{
				QString notes = i->second;
				ui->notesEdit->setPlainText(notes);
			}
		}

		if(lookup(uri.path()))
		{
		
			this->URIHandled = true;
			if(foundQtyParam)
			{
				acceptOffer();
			}
			this->URIHandled = false;
		}

	}
	catch (Object& objError)
	{
		strError = find_value(objError, "message").get_str();
		QMessageBox::critical(this, windowTitle(),
		tr("Error in offer URI Handler: \"%1\"").arg(QString::fromStdString(strError)),
			QMessageBox::Ok, QMessageBox::Ok);
		this->URIHandled = false;
		return false;
	}
	catch(std::exception& e)
	{
		strError = e.what();
		QMessageBox::critical(this, windowTitle(),
		tr("Exception in offer URI Handler: \"%1\"").arg(QString::fromStdString(strError)),
			QMessageBox::Ok, QMessageBox::Ok);
		this->URIHandled = false;
		return false;
	}

    return true;
}
void AcceptandPayOfferListPage::setValue(COffer &offer)
{

    ui->offeridEdit->setText(QString::fromStdString(stringFromVch(offer.vchRand)));
	ui->infoTitle->setText(QString::fromStdString(stringFromVch(offer.sTitle)));
	ui->infoCategory->setText(QString::fromStdString(stringFromVch(offer.sCategory)));
	ui->infoCurrency->setText(QString::fromStdString(stringFromVch(offer.sCurrencyCode)));
	ui->infoPrice->setText(QString::number(offer.GetPrice()));
	ui->infoQty->setText(QString::number(offer.nQty));
	ui->infoDescription->setText(QString::fromStdString(stringFromVch(offer.sDescription)));
	ui->infoPaymentAddress->setText(QString::fromStdString(stringFromVch(offer.vchPaymentAddress)));
	ui->qtyEdit->setText(QString("1"));

}

bool AcceptandPayOfferListPage::handleURI(const QString& strURI)
{
	QString URI = strURI;
	if(URI.startsWith("syscoin://"))
    {
        URI.replace(0, 11, "syscoin:");
    }
    QUrl uriInstance(URI);
    return handleURI(uriInstance);
}

